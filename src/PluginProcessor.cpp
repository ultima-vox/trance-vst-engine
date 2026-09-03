#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "preset/PresetManager.h"
#include "export/MidiExport.h"
#include <cmath>
#include <random>

namespace {
void seedSequence(vstengine::generator::Sequence& seq, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < seq.size(); ++i) {
        auto& step = seq[i];
        step.gate = (i % 4) != 0;
        step.accent = (i % 4) == 1;
        step.noteOffset = 0;
        step.velocity = step.accent ? 0.95f : 0.72f;
        step.probability = 1.0f;
        step.ratchetCount = 1;
        step.slideDuration = 0.0f;
        step.gateWidth = 0.75f;
        
        // darkPsy style: occasional octave up on gate steps
        if (step.gate && dist(rng) < 0.12f)
            step.noteOffset = 12;
    }
}
} // anonymous namespace

VstEngineAudioProcessor::VstEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      sequenceData(16)
{
    // Monophonic bass engine: a single voice guarantees that consecutive
    // generated notes always land on the same voice, so legato glide is
    // deterministic (the voice receiving the target note always carries the
    // source pitch state). Polyphony would let JUCE assign the new note to a
    // different free voice and silently drop the glide.
    synth.addVoice(new vstengine::dsp::PsyBassVoice());

    synth.addSound(new vstengine::dsp::PsyBassSound());

    // Seed initial sequence with darkPsy pattern
    seedInitialSequence();

    presetManager_ = std::make_unique<vstengine::PresetManager>(*this, sequenceData);
}

void VstEngineAudioProcessor::prepareToPlay(const double sampleRate, const int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    resetPlaybackState();

    // Preallocate the keyboard MIDI scratch buffer using JUCE's intended API.
    // Budget: 4096 bytes covers a dense block of keyboard events (256 events
    // at ~16 bytes/event sysex headroom) with safety margin. processBlock()
    // then only clears and reuses — no allocation or growth on the realtime
    // thread.
    keyboardMidiScratch.ensureSize(4096);
}

void VstEngineAudioProcessor::resetPlaybackState()
{
    currentStep = 0;
    heldNote = -1;
    heldChannel = 1;
    samplesUntilNextStep = 0.0;
    samplesUntilNoteOff = -1.0;
    ratchetsRemaining = 0;
    samplesUntilNextRatchet = 0.0;
    subNoteDuration = 0.0;
    lastGeneratedNote = -1;
    playedStepCounter = 0;
    transportWasPlaying = false;
    playHeadStep = -1;
    clearVoiceGlideRequests();

    // Deterministic probability RNG: seeded from the rngSeed parameter so the
    // same project state always reproduces the same probability decisions from
    // the same playback start point.
    probabilityState.state =
        static_cast<std::uint32_t>(apvts.getRawParameterValue("rngSeed")->load());
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

    // Invalidate glide requests from the previous block before new ones are
    // scheduled (same thread, same block ordering — no synchronization needed).
    clearVoiceGlideRequests();

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
        ratchetsRemaining = 0;
        lastGeneratedNote = -1;
    }

    if (useGenerator)
        addGeneratedMidi(midi, buffer.getNumSamples(), channel, rootNote);
    else
        playHeadStep = -1;

    for (const auto metadata : midi)
        midiKeyboardState.processNextMidiEvent(metadata.getMessage());

    // Use the preallocated member scratch buffer: clear, fill, merge. No
    // local MidiBuffer is constructed and the scratch buffer's capacity was
    // reserved in prepareToPlay, so this path performs no heap allocation on
    // the realtime thread.
    keyboardMidiScratch.clear();
    midiKeyboardState.processNextMidiBuffer(
        keyboardMidiScratch, 0, buffer.getNumSamples(), true);
    midi.addEvents(keyboardMidiScratch, 0, buffer.getNumSamples(), 0);

    syncVoiceParameters();

    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
}

void VstEngineAudioProcessor::syncVoiceParameters()
{
    const auto drive = apvts.getRawParameterValue("drive")->load();
    const auto release = apvts.getRawParameterValue("release")->load();
    const auto ampAttack = apvts.getRawParameterValue("ampAttack")->load();
    const auto ampDecay = apvts.getRawParameterValue("ampDecay")->load();
    const auto ampSustain = apvts.getRawParameterValue("ampSustain")->load();
    const auto pitchEnvAmount =
        apvts.getRawParameterValue("pitchEnvAmount")->load();
    const auto pitchEnvTime = apvts.getRawParameterValue("pitchEnvTime")->load();
    const auto pitchEnvCurve = apvts.getRawParameterValue("pitchEnvCurve")->load();
    const auto filterCutoff = apvts.getRawParameterValue("filterCutoff")->load();
    const auto filterResonance =
        apvts.getRawParameterValue("filterResonance")->load();
    const auto filterDrive = apvts.getRawParameterValue("filterDrive")->load();
    const auto keyTracking = apvts.getRawParameterValue("keyTracking")->load();
    const auto outputLevel = apvts.getRawParameterValue("outputLevel")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::dsp::PsyBassVoice*>(synth.getVoice(i))) {
            voice->setDrive(drive);
            voice->setAmpRelease(release);
            voice->setAmpAttack(ampAttack);
            voice->setAmpDecay(ampDecay);
            voice->setAmpSustain(ampSustain);
            voice->setPitchEnvelopeAmount(pitchEnvAmount);
            voice->setPitchEnvelopeTime(pitchEnvTime);
            voice->setPitchEnvelopeCurve(pitchEnvCurve);
            voice->setFilterCutoff(filterCutoff);
            voice->setFilterResonance(filterResonance);
            voice->setFilterDrive(filterDrive);
            voice->setKeyTracking(keyTracking);
            voice->setOutputLevel(outputLevel);
        }
    }
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

    if (auto* playHeadProvider = getPlayHead()) {
        if (const auto position = playHeadProvider->getPosition()) {
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
        ratchetsRemaining = 0;
        lastGeneratedNote = -1;
        playHeadStep = -1;
        transportWasPlaying = false;
        return;
    }

    // Deterministic probability RNG: reseeded on every transport restart so
    // playback from the same project state reproduces the same decisions.
    if (!transportWasPlaying) {
        transportWasPlaying = true;
        playedStepCounter = 0;
        lastGeneratedNote = -1;
        probabilityState.state = static_cast<std::uint32_t>(
            apvts.getRawParameterValue("rngSeed")->load());
    }

    const auto samplesPerQuarter =
        currentSampleRate * 60.0 / juce::jmax(20.0, bpm);

    // Canonical timing: step duration always derives from the sequence's
    // timing mode (same formula as MIDI export).
    const auto samplesPerStep =
        samplesPerQuarter / stepsPerQuarterNote(sequenceData.getTimingMode());
    const auto seqSize = sequenceData.size();

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

        // Ratchet sub-notes: retrigger the step note evenly within the step.
        if (ratchetsRemaining > 0) {
            samplesUntilNextRatchet -= 1.0;

            if (samplesUntilNextRatchet <= 0.0) {
                const auto& step =
                    sequenceData[(currentStep + seqSize - 1) % seqSize];
                triggerGeneratedNote(midi, offset, channel, rootNote, step,
                                     samplesPerStep, subNoteDuration);
                --ratchetsRemaining;
                samplesUntilNextRatchet += subNoteDuration;
            }
        }

        samplesUntilNextStep -= 1.0;

        if (samplesUntilNextStep <= 0.0) {
            playHeadStep = currentStep;
            const auto& step = sequenceData[static_cast<int>(currentStep)];

            // Advance RNG once per step regardless of gate state, then evaluate
            // probability. Uses the shared shouldPlayStep rule so realtime +
            // export produce identical pass/skip decisions for the same seed.
            ++playedStepCounter;
            const bool passesProbability = vstengine::generator::shouldPlayStep(probabilityState, step.probability);

            if (passesProbability && step.gate) {
                const auto ratchet = juce::jlimit(1, 8, step.ratchetCount);
                subNoteDuration = samplesPerStep / static_cast<double>(ratchet);
                triggerGeneratedNote(midi, offset, channel, rootNote, step,
                                     samplesPerStep, subNoteDuration);
                ratchetsRemaining = ratchet - 1;
                samplesUntilNextRatchet = subNoteDuration;
            }

            currentStep = (currentStep + 1) % seqSize;
            samplesUntilNextStep += samplesPerStep;
        }
    }
}

void VstEngineAudioProcessor::triggerGeneratedNote(juce::MidiBuffer& midi,
                                                   const int offset,
                                                   const int channel,
                                                   const int rootNote,
                                                   const vstengine::generator::Step& step,
                                                   const double samplesPerStep,
                                                   const double subNoteSamples)
{
    heldChannel = channel;
    heldNote = juce::jlimit(0, 127, rootNote + step.noteOffset);
    const auto velocity =
        step.accent ? 0.95f : juce::jlimit(0.0f, 1.0f, step.velocity);

    // Slide semantics: slideDuration is measured in sequence steps. A note is
    // slide-eligible when it directly follows an audible generated note in the
    // playback stream (previous step's note or previous ratchet sub-note) with
    // a different pitch. The synth voice then glides from its current pitch to
    // this note's pitch over slideDuration * stepDuration.
    if (step.slideDuration > 0.0f && lastGeneratedNote >= 0
        && lastGeneratedNote != heldNote) {
        requestVoiceGlide(heldNote, static_cast<float>(
            step.slideDuration * samplesPerStep / currentSampleRate));
    }

    midi.addEvent(
        juce::MidiMessage::noteOn(heldChannel, heldNote, velocity), offset);
    lastGeneratedNote = heldNote;

    // Sub-note gate never overlaps the next sub-note (gateWidth <= 1), so
    // generated notes can never get stuck.
    samplesUntilNoteOff =
        subNoteSamples * juce::jlimit(0.0, 1.0, static_cast<double>(step.gateWidth));
}

void VstEngineAudioProcessor::requestVoiceGlide(const int noteNumber,
                                                const float glideSeconds) noexcept
{
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::dsp::PsyBassVoice*>(synth.getVoice(i)))
            voice->requestGlide(noteNumber, glideSeconds);
    }
}

void VstEngineAudioProcessor::clearVoiceGlideRequests() noexcept
{
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::dsp::PsyBassVoice*>(synth.getVoice(i)))
            voice->clearPendingGlide();
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

    // Seed for the deterministic probability RNG used by realtime playback.
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "rngSeed", 1 },
        "Generator Seed",
        0, 2147483647, 1234));

    // --- Resonant low-pass filter ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterCutoff", 1 }, "Filter Cutoff",
        juce::NormalisableRange<float> { 0.02f, 0.95f, 0.001f, 0.4f },
        0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterResonance", 1 }, "Filter Resonance",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterDrive", 1 }, "Filter Drive",
        juce::NormalisableRange<float> { 0.0f, 4.0f, 0.01f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "keyTracking", 1 }, "Key Tracking",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.2f));

    // --- Per-parameter amp ADSR (release is the existing "release" knob) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampAttack", 1 }, "Amp Attack",
        juce::NormalisableRange<float> { 0.0005f, 0.100f, 0.0005f, 0.3f },
        0.001f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampDecay", 1 }, "Amp Decay",
        juce::NormalisableRange<float> { 0.005f, 1.0f, 0.001f, 0.4f },
        0.055f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampSustain", 1 }, "Amp Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.72f));

    // --- Pitch envelope ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitchEnvAmount", 1 }, "Pitch Env Amount",
        juce::NormalisableRange<float> { 0.0f, 36.0f, 0.1f },
        12.0f, "st"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitchEnvTime", 1 }, "Pitch Env Time",
        juce::NormalisableRange<float> { 0.001f, 0.5f, 0.001f, 0.4f },
        0.018f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "pitchEnvCurve", 1 }, "Pitch Env Curve",
        juce::NormalisableRange<float> { 0.5f, 8.0f, 0.1f },
        2.0f));

    // --- Output gain compensation ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputLevel", 1 }, "Output Level",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.01f, 0.6f },
        1.0f));

    return { params.begin(), params.end() };
}


juce::File VstEngineAudioProcessor::createGeneratedMidiFile()
{
    // Export goes through the shared MIDI export builder so timing mode,
    // ratchet and slide behave exactly as documented and identically to
    // realtime playback.
    vstengine::midiexport::Options options;
    options.channel = juce::jlimit(
        1, 16,
        static_cast<int>(apvts.getRawParameterValue("midiChannel")->load()));
    options.rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));
    options.seed = static_cast<std::uint32_t>(
        apvts.getRawParameterValue("rngSeed")->load());

    const auto track =
        vstengine::midiexport::buildSequenceTrack(sequenceData, options);

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(
        static_cast<int>(options.ticksPerQuarter));
    midiFile.addTrack(track);

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
    auto tree = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(tree.createXml());
    
    // Encode sequence into state
    juce::MemoryBlock mb;
    sequenceData.serialize(mb);
    xml->setAttribute("sequenceData", mb.toBase64Encoding());
    
    copyXmlToBinary(*xml, destData);
}

void VstEngineAudioProcessor::setStateInformation(const void* data,
                                                    const int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        
        // Restore sequence from state
        const auto seqBase64 = xml->getStringAttribute("sequenceData", {});
        if (!seqBase64.isEmpty()) {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding(seqBase64)) {
                sequenceData = vstengine::generator::Sequence::deserialize(mb);
            }
        }
    }
}

void VstEngineAudioProcessor::seedInitialSequence()
{
    sequenceData.clear();
    seedSequence(sequenceData, 0xD4A4u);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VstEngineAudioProcessor();
}
