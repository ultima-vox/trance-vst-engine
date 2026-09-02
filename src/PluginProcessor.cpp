#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

VstEngineAudioProcessor::VstEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      sequence(vstengine::generator::SequenceLength::steps16)
{
    // Initialize a basic dark-psy rolling pattern
    for (size_t i = 0; i < sequence.size(); ++i) {
        auto& step = sequence[i];
        step.gate = (i % 4) != 0;  // quarter-note rests for kick
        step.accent = (i % 4) == 1;
        step.velocity = step.accent ? 0.95f : 0.72f;
    }

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

    syncVoiceParameters();

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
            const auto& step = sequence[static_cast<size_t>(currentStep)];

            if (step.gate) {
                heldChannel = channel;
                heldNote = juce::jlimit(0, 127, rootNote + step.noteOffset);
                const auto velocity = step.accent ? 0.95f : 0.72f;

                midi.addEvent(
                    juce::MidiMessage::noteOn(
                        heldChannel, heldNote, velocity),
                    offset);

                samplesUntilNoteOff = samplesPerStep * step.gateWidth;
            }

            currentStep = (currentStep + 1)
                % static_cast<int>(sequence.size());
            samplesUntilNextStep += samplesPerStep;
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
VstEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- Distortion ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> { 1.0f, 6.0f, 0.01f }, 1.8f));

    // --- Pitch envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitchEnvAmount", 1 }, "Pitch Env Amount",
        juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f }, 12.0f, "st"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitchEnvTime", 1 }, "Pitch Env Time",
        juce::NormalisableRange<float> { 0.005f, 0.2f, 0.001f, 0.5f },
        0.018f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitchEnvCurve", 1 }, "Pitch Env Curve",
        juce::NormalisableRange<float> { 0.5f, 5.0f, 0.1f }, 2.0f));

    // --- ADSR ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampAttack", 1 }, "Attack",
        juce::NormalisableRange<float> { 0.001f, 1.0f, 0.001f, 0.5f },
        0.001f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampDecay", 1 }, "Decay",
        juce::NormalisableRange<float> { 0.01f, 2.0f, 0.001f, 0.5f },
        0.055f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampSustain", 1 }, "Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.72f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampRelease", 1 }, "Release",
        juce::NormalisableRange<float> { 0.005f, 0.250f, 0.001f, 0.5f },
        0.035f, "s"));

    // --- Filter ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterCutoff", 1 }, "Cutoff",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterResonance", 1 }, "Resonance",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterDrive", 1 }, "Filter Drive",
        juce::NormalisableRange<float> { 0.0f, 5.0f, 0.01f }, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "keyTracking", 1 }, "Key Tracking",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // --- Output ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputLevel", 1 }, "Output Level",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.001f }, 1.0f));

    // --- MIDI ---
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

void VstEngineAudioProcessor::syncVoiceParameters()
{
    const auto drive = apvts.getRawParameterValue("drive")->load();
    const auto release = apvts.getRawParameterValue("ampRelease")->load();
    const auto pitchEnvAmount =
        apvts.getRawParameterValue("pitchEnvAmount")->load();
    const auto pitchEnvTime =
        apvts.getRawParameterValue("pitchEnvTime")->load();
    const auto pitchEnvCurve =
        apvts.getRawParameterValue("pitchEnvCurve")->load();
    const auto ampAttack =
        apvts.getRawParameterValue("ampAttack")->load();
    const auto ampDecay =
        apvts.getRawParameterValue("ampDecay")->load();
    const auto ampSustain =
        apvts.getRawParameterValue("ampSustain")->load();
    const auto filterCutoff =
        apvts.getRawParameterValue("filterCutoff")->load();
    const auto filterResonance =
        apvts.getRawParameterValue("filterResonance")->load();
    const auto filterDrive =
        apvts.getRawParameterValue("filterDrive")->load();
    const auto keyTracking =
        apvts.getRawParameterValue("keyTracking")->load();
    const auto outputLevel =
        apvts.getRawParameterValue("outputLevel")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::dsp::PsyBassVoice*>(synth.getVoice(i))) {
            voice->setDrive(drive);
            voice->setAmpRelease(release);
            voice->setPitchEnvelopeAmount(pitchEnvAmount);
            voice->setPitchEnvelopeTime(pitchEnvTime);
            voice->setPitchEnvelopeCurve(pitchEnvCurve);
            voice->setAmpAttack(ampAttack);
            voice->setAmpDecay(ampDecay);
            voice->setAmpSustain(ampSustain);
            voice->setFilterCutoff(filterCutoff);
            voice->setFilterResonance(filterResonance);
            voice->setFilterDrive(filterDrive);
            voice->setKeyTracking(keyTracking);
            voice->setOutputLevel(outputLevel);
        }
    }
}


juce::File VstEngineAudioProcessor::createGeneratedMidiFile()
{
    constexpr int ticksPerQuarter = 960;
    constexpr double ticksPerStep = ticksPerQuarter / 4.0;

    const auto channel = juce::jlimit(
        1, 16,
        static_cast<int>(apvts.getRawParameterValue("midiChannel")->load()));
    const auto rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));

    juce::MidiMessageSequence sequenceMidi;

    for (size_t i = 0; i < sequence.size(); ++i) {
        const auto& step = sequence[i];

        if (!step.gate)
            continue;

        const auto note = juce::jlimit(0, 127, rootNote + step.noteOffset);
        const auto velocity = step.accent ? 0.95f : 0.72f;
        const auto startTick = static_cast<double>(i) * ticksPerStep;
        const auto endTick = startTick + ticksPerStep * step.gateWidth;

        auto noteOn = juce::MidiMessage::noteOn(channel, note, velocity);
        noteOn.setTimeStamp(startTick);
        sequenceMidi.addEvent(noteOn);

        auto noteOff = juce::MidiMessage::noteOff(channel, note);
        noteOff.setTimeStamp(endTick);
        sequenceMidi.addEvent(noteOff);
    }

    auto endOfTrack = juce::MidiMessage::endOfTrack();
    endOfTrack.setTimeStamp(
        static_cast<double>(sequence.size()) * ticksPerStep);
    sequenceMidi.addEvent(endOfTrack);
    sequenceMidi.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(ticksPerQuarter);
    midiFile.addTrack(sequenceMidi);

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
    juce::MemoryBlock state;
    
    // Serialize sequence
    sequence.serialize(state);
    
    // Append APVTS state
    if (auto apvtsXml = apvts.copyState().createXml()) {
        juce::MemoryBlock apvtsData;
        copyXmlToBinary(*apvtsXml, apvtsData);
        state.append(apvtsData.getData(), apvtsData.getSize());
    }
    
    destData = state;
}

void VstEngineAudioProcessor::setStateInformation(const void* data,
                                                   const int sizeInBytes)
{
    juce::MemoryBlock mb(data, sizeInBytes);
    
    // Deserialize sequence (first 12+ bytes)
    if (mb.getSize() >= 12) {
        sequence = vstengine::generator::Sequence::deserialize(mb);
    }
    
    // Deserialize APVTS state (remaining bytes)
    const size_t seqSize = 12 + sequence.size() * 29;
    if (mb.getSize() > static_cast<ssize_t>(seqSize)) {
        const void* apvtsData = mb.getData() + seqSize;
        const int apvtsSize = static_cast<int>(mb.getSize() - seqSize);
        if (auto xml = getXmlFromBinary(apvtsData, apvtsSize))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VstEngineAudioProcessor();
}
