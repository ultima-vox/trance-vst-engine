#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

VstEngineAudioProcessor::VstEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      pattern(vstengine::generator::PatternGenerator::generate(
          vstengine::generator::Style::darkPsy, 0xD4A4u))
{
    for (int i = 0; i < 4; ++i)
        synth.addVoice(new vstengine::dsp::PsyBassVoice());

    synth.addSound(new vstengine::dsp::PsyBassSound());
}

void VstEngineAudioProcessor::prepareToPlay(const double sampleRate, const int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    currentStep = 0;
    heldNote = -1;
    heldChannel = 1;
    samplesUntilNextStep = 0.0;
    samplesUntilNoteOff = -1.0;
}

bool VstEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void VstEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto incomingHasNotes = containsNoteEvents(midi);
    const auto keyboardHasNotes = keyboardHasActiveNotes();
    const auto mode = currentMidiMode();
    const auto channel = juce::jlimit(
        1, 16,
        static_cast<int>(apvts.getRawParameterValue("midiChannel")->load()));
    const auto rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));

    bool useGenerator = false;

    switch (mode) {
        case MidiSourceMode::autoDetect:
            useGenerator = !(incomingHasNotes || keyboardHasNotes);
            break;
        case MidiSourceMode::pianoRoll:
            useGenerator = false;
            break;
        case MidiSourceMode::generator:
            midi.clear();
            useGenerator = true;
            break;
        case MidiSourceMode::both:
            useGenerator = true;
            break;
    }

    if (!useGenerator && heldNote >= 0) {
        midi.addEvent(
            juce::MidiMessage::noteOff(heldChannel, heldNote), 0);
        heldNote = -1;
        samplesUntilNoteOff = -1.0;
        samplesUntilNextStep = 0.0;
    }

    if (useGenerator)
        addGeneratedMidi(midi, buffer.getNumSamples(), channel, rootNote);

    for (const auto metadata : midi)
        midiKeyboardState.processNextMidiEvent(metadata.getMessage());

    juce::MidiBuffer keyboardMidi;
    midiKeyboardState.processNextMidiBuffer(
        keyboardMidi, 0, buffer.getNumSamples(), true);
    midi.addEvents(
        keyboardMidi, 0, buffer.getNumSamples(), 0);

    const auto drive = apvts.getRawParameterValue("drive")->load();
    const auto release = apvts.getRawParameterValue("release")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::dsp::PsyBassVoice*>(synth.getVoice(i))) {
            voice->setDrive(drive);
            voice->setRelease(release);
        }
    }

    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
}

bool VstEngineAudioProcessor::containsNoteEvents(
    const juce::MidiBuffer& midi) noexcept
{
    for (const auto metadata : midi) {
        const auto message = metadata.getMessage();
        if (message.isNoteOnOrOff())
            return true;
    }

    return false;
}

bool VstEngineAudioProcessor::keyboardHasActiveNotes() const noexcept
{
    for (int note = 0; note < 128; ++note) {
        if (midiKeyboardState.isNoteOnForChannels(0xffff, note))
            return true;
    }

    return false;
}

VstEngineAudioProcessor::MidiSourceMode
VstEngineAudioProcessor::currentMidiMode() const noexcept
{
    const auto raw = apvts.getRawParameterValue("midiMode")->load();
    const auto index = juce::jlimit(0, 3, static_cast<int>(std::lround(raw)));
    return static_cast<MidiSourceMode>(index);
}

void VstEngineAudioProcessor::addGeneratedMidi(juce::MidiBuffer& midi,
                                                const int numSamples,
                                                const int channel,
                                                const int rootNote)
{
    double bpm = 145.0;
    bool playing = true;

    if (auto* playHead = getPlayHead()) {
        if (const auto position = playHead->getPosition()) {
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            playing = position->getIsPlaying();
        }
    }

    if (!playing) {
        if (heldNote >= 0)
            midi.addEvent(
                juce::MidiMessage::noteOff(heldChannel, heldNote), 0);

        heldNote = -1;
        samplesUntilNoteOff = -1.0;
        samplesUntilNextStep = 0.0;
        return;
    }

    const auto samplesPerQuarter =
        currentSampleRate * 60.0 / juce::jmax(20.0, bpm);
    const auto samplesPerStep = samplesPerQuarter / 4.0;

    for (int offset = 0; offset < numSamples; ++offset) {
        if (samplesUntilNoteOff >= 0.0) {
            samplesUntilNoteOff -= 1.0;

            if (samplesUntilNoteOff <= 0.0 && heldNote >= 0) {
                midi.addEvent(
                    juce::MidiMessage::noteOff(heldChannel, heldNote), offset);
                heldNote = -1;
                samplesUntilNoteOff = -1.0;
            }
        }

        samplesUntilNextStep -= 1.0;

        if (samplesUntilNextStep <= 0.0) {
            const auto& step = pattern[static_cast<std::size_t>(currentStep)];

            if (step.gate) {
                heldChannel = channel;
                heldNote = juce::jlimit(0, 127, rootNote + step.noteOffset);
                const auto velocity = step.accent ? 0.95f : 0.72f;

                midi.addEvent(
                    juce::MidiMessage::noteOn(
                        heldChannel, heldNote, velocity),
                    offset);

                samplesUntilNoteOff = samplesPerStep * 0.62;
            }

            currentStep = (currentStep + 1)
                % static_cast<int>(pattern.size());
            samplesUntilNextStep += samplesPerStep;
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
VstEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> { 1.0f, 6.0f, 0.01f }, 1.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "release", 1 }, "Release",
        juce::NormalisableRange<float> { 0.005f, 0.250f, 0.001f, 0.5f },
        0.035f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "midiMode", 1 },
        "MIDI Source",
        juce::StringArray { "Auto", "Piano Roll", "Generator", "Both" },
        0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "midiChannel", 1 },
        "Generator MIDI Channel",
        1, 16, 1));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "rootNote", 1 },
        "Generator Root Note",
        24, 60, 36));

    return { params.begin(), params.end() };
}


juce::File VstEngineAudioProcessor::createGeneratedMidiFile()
{
    constexpr int ticksPerQuarter = 960;
    constexpr double ticksPerStep = ticksPerQuarter / 4.0;
    constexpr double gateRatio = 0.62;

    const auto channel = juce::jlimit(
        1, 16,
        static_cast<int>(apvts.getRawParameterValue("midiChannel")->load()));
    const auto rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));

    juce::MidiMessageSequence sequence;

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const auto& step = pattern[i];

        if (!step.gate)
            continue;

        const auto note = juce::jlimit(0, 127, rootNote + step.noteOffset);
        const auto velocity = step.accent ? 0.95f : 0.72f;
        const auto startTick = static_cast<double>(i) * ticksPerStep;
        const auto endTick = startTick + ticksPerStep * gateRatio;

        auto noteOn = juce::MidiMessage::noteOn(channel, note, velocity);
        noteOn.setTimeStamp(startTick);
        sequence.addEvent(noteOn);

        auto noteOff = juce::MidiMessage::noteOff(channel, note);
        noteOff.setTimeStamp(endTick);
        sequence.addEvent(noteOff);
    }

    auto endOfTrack = juce::MidiMessage::endOfTrack();
    endOfTrack.setTimeStamp(
        static_cast<double>(pattern.size()) * ticksPerStep);
    sequence.addEvent(endOfTrack);
    sequence.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(ticksPerQuarter);
    midiFile.addTrack(sequence);

    const auto tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("VST-Engine-Generated.mid");

    tempFile.deleteFile();

    if (auto stream = tempFile.createOutputStream()) {
        if (midiFile.writeTo(*stream))
            return tempFile;
    }

    return {};
}

void VstEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void VstEngineAudioProcessor::setStateInformation(const void* data,
                                                   const int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VstEngineAudioProcessor();
}
