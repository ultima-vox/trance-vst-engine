#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "core/KickParameterIds.h"
#include "midi/GeneratedNoteScheduler.h"
#include "midi/MidiExport.h"
#include "preset/PresetManager.h"
#include <cmath>

VstEngineAudioProcessor::VstEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      sequenceData(16),
      presetStore(apvts)
{
    // Monophonic bass engine: a single voice guarantees that consecutive
    // generated notes always land on the same voice, so legato glide is
    // deterministic (the voice receiving the target note always carries the
    // source pitch state). Polyphony would let JUCE assign the new note to a
    // different free voice and silently drop the glide.
    synth.addVoice(new vstengine::bass::PsyBassVoice());

    synth.addSound(new vstengine::bass::PsyBassSound());

    // Seed initial sequence with darkPsy pattern (canonical seeded operation
    // owned by the sequence module; same generator stream as before).
    seedInitialSequence();
    sequenceBridge.publish(sequenceData);
    (void) sequenceBridge.read(audioSequenceData);

    presetManager_ = std::make_unique<vstengine::PresetManager>(
        presetStore, sequenceData);
}

void VstEngineAudioProcessor::prepareToPlay(const double sampleRate, const int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);

    // Reset the generated-playback scheduler and reseed the deterministic
    // probability RNG from the rngSeed parameter so the same project state
    // always reproduces the same probability decisions from the same playback
    // start point.
    scheduler.reset(sampleRate);
    scheduler.reseedProbability(static_cast<std::uint32_t>(
        apvts.getRawParameterValue("rngSeed")->load()));

    // Preallocate the keyboard MIDI scratch buffer using JUCE's intended API.
    // Budget: 4096 bytes covers a dense block of keyboard events (256 events
    // at ~16 bytes/event sysex headroom) with safety margin. processBlock()
    // then only clears and reuses — no allocation or growth on the realtime
    // thread.
    keyboardMidiScratch.ensureSize(4096);

    // Kick engine: reset voice state for the new rate and preallocate the
    // scratch buffer used to strip kick-channel note events. Extra capacity
    // covers all fixed pending-delay slots becoming due in one block without
    // growing the buffer on the audio thread.
    kickSynth.prepare(sampleRate);
    partMidiRouter.reset();
    wasTransportPlaying = false;
    kickMidiScratch.ensureSize(
        4096 + vstengine::midi::PartMidiRouter::maxPendingEvents * 16);
}

void VstEngineAudioProcessor::releaseResources()
{
    synth.allNotesOff(0, false);
    kickSynth.reset();
    partMidiRouter.reset();
    scheduler.reset(currentSampleRate);
    wasTransportPlaying = false;
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
    (void) sequenceBridge.read(audioSequenceData);

    // Explicit transport safety path, separate from ordinary note-off.
    if (auto* hostPlayHead = getPlayHead()) {
        if (const auto position = hostPlayHead->getPosition()) {
            const bool playing = position->getIsPlaying();
            if (wasTransportPlaying && !playing) {
                synth.allNotesOff(0, false);
                kickSynth.release(0);
                partMidiRouter.reset();
            }
            wasTransportPlaying = playing;
        }
    }

    // Invalidate glide requests from the previous block before new ones are
    // scheduled (same thread, same block ordering — no synchronization needed).
    clearVoiceGlideRequests();

    // MIDI source isolation: the generator decision only sees EXTERNAL host
    // notes (note events present in this block's incoming buffer — computed
    // before anything is appended) and GUI keyboard notes. Internally
    // generated MIDI is never an input observation, so it can never suppress
    // generator playback in AUTO mode.
    const auto incomingHasNotes = containsNoteEvents(midi);
    const auto keyboardHasNotes = keyboardHasActiveNotes();
    const auto mode = currentMidiMode();
    const auto channel = juce::jlimit(
        1, 16,
        static_cast<int>(apvts.getRawParameterValue("midiChannel")->load()));
    const auto rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));

    // GENERATOR mode drops incoming host MIDI (only generated material plays).
    // GUI keyboard notes intentionally still work in GENERATOR mode, matching
    // the documented PIANO ROLL / GENERATOR / BOTH semantics.
    if (mode == MidiSourceMode::generator)
        midi.clear();

    const bool useGenerator = vstengine::midi::shouldRunGenerator(
        mode, incomingHasNotes, keyboardHasNotes);

    // Generated material: the scheduler (libs/midi) owns the full playback
    // state machine — PPQ-aligned scheduling, the free-running fallback,
    // probability rolls, ratchet tails and flush semantics. The shell only
    // bridges host transport observations (playhead reads are host-specific)
    // and applies the scheduler's glide requests to the bass voices.
    if (useGenerator) {
        vstengine::midi::TransportFrame frame;
        frame.playing = true;
        frame.havePpq = false;
        frame.ppqAtBlockStart = 0.0;
        frame.bpm = 145.0;

        if (auto* playHeadProvider = getPlayHead()) {
            if (const auto position = playHeadProvider->getPosition()) {
                if (const auto hostBpm = position->getBpm())
                    frame.bpm = *hostBpm;
                frame.playing = position->getIsPlaying();
                if (const auto hostPpq = position->getPpqPosition()) {
                    frame.ppqAtBlockStart = *hostPpq;
                    frame.havePpq = true;
                }
            }
        }

        scheduler.process(midi, audioSequenceData, frame, buffer.getNumSamples(),
                          currentSampleRate, channel, rootNote,
                          static_cast<std::uint32_t>(
                              apvts.getRawParameterValue("rngSeed")->load()));

        for (int i = 0; i < scheduler.numGlideRequests(); ++i) {
            const auto request = scheduler.glideRequest(i);
            requestVoiceGlide(request.noteNumber, request.glideSeconds);
        }
        scheduler.clearGlideRequests();
    } else {
        scheduler.flush(midi, 0);
    }

    // GUI keyboard input: the keyboard state deliberately receives ONLY the
    // notes the user plays on the on-screen keyboard. Host and generated MIDI
    // reach the synth directly from the block buffer and are never fed into
    // the keyboard state, so "generated notes highlight the keyboard" can
    // never suppress generator playback in AUTO mode.
    keyboardMidiScratch.clear();
    midiKeyboardState.processNextMidiBuffer(
        keyboardMidiScratch, 0, buffer.getNumSamples(), true);
    midi.addEvents(keyboardMidiScratch, 0, buffer.getNumSamples(), 0);

    if (panicRequested.exchange(false)) {
        midi.clear();
        synth.allNotesOff(0, false);
        kickSynth.release(0);
        partMidiRouter.reset();
        scheduler.reset(currentSampleRate);
    }

    // Part-aware routing lives in vst_midi. Kick is one-shot: note-offs are
    // consumed without release. Only kick-channel CC120/123 use panic release.
    // Bass Part 1 / CH1 alone receives the match delay; its fixed queue carries
    // events across blocks without allocation or locks.
    {
        const auto kickChannel = juce::jlimit(
            1, 16, static_cast<int>(
                       apvts.getRawParameterValue("kickMidiChannel")->load()));

        const int bassTimingOffsetSamples = static_cast<int>(std::lround(
            juce::jlimit(
                0, 16,
                static_cast<int>(
                    apvts.getRawParameterValue("matchBassTimingOffsetMs")
                        ->load()))
            * currentSampleRate / 1000.0));
        partMidiRouter.process(midi, kickMidiScratch, buffer.getNumSamples(),
                               1, kickChannel, bassTimingOffsetSamples);
        midi.swapWith(kickMidiScratch);

        for (int i = 0; i < partMidiRouter.numKickActions(); ++i) {
            const auto& action = partMidiRouter.kickAction(i);
            if (action.type
                == vstengine::midi::PartMidiRouter::KickActionType::trigger)
                kickSynth.trigger(action.velocity, action.note,
                                  action.samplePosition);
            else
                kickSynth.release(action.samplePosition);
        }
    }

    syncVoiceParameters();
    syncKickParameters();

    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    // The kick renders after the bass synth so a note-on and the voice it
    // triggers land in the same block (sample-accurate offsets are preserved
    // by the kick's internal trigger queue).
    kickSynth.render(buffer, buffer.getNumSamples());
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
                dynamic_cast<vstengine::bass::PsyBassVoice*>(synth.getVoice(i))) {
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

void VstEngineAudioProcessor::syncKickParameters()
{
    vstengine::kick::KickParams p;
    p.pitchStart = apvts.getRawParameterValue("kickPitchStart")->load();
    p.pitchEnd = apvts.getRawParameterValue("kickPitchEnd")->load();
    p.pitchDecay = apvts.getRawParameterValue("kickPitchDecay")->load();
    p.pitchCurve = apvts.getRawParameterValue("kickPitchCurve")->load();
    p.bodyDecay = apvts.getRawParameterValue("kickBodyDecay")->load();
    p.tail = apvts.getRawParameterValue("kickTail")->load();
    p.click = apvts.getRawParameterValue("kickClick")->load();
    p.clickTone = apvts.getRawParameterValue("kickClickTone")->load();
    p.drive = apvts.getRawParameterValue("kickDrive")->load();
    p.clip = apvts.getRawParameterValue("kickClip")->load();
    p.transient = apvts.getRawParameterValue("kickTransient")->load();
    p.sub = apvts.getRawParameterValue("kickSub")->load();
    p.tune = apvts.getRawParameterValue("kickTune")->load();
    p.phase = apvts.getRawParameterValue("kickPhase")->load();
    p.outputLevel = apvts.getRawParameterValue("kickOutputLevel")->load();

    kickSynth.setParameters(p);
}

vstengine::match::MatchReport VstEngineAudioProcessor::analyzeKickBassMatch()
{
    vstengine::kick::KickParams kick;
    kick.pitchStart = apvts.getRawParameterValue("kickPitchStart")->load();
    kick.pitchEnd = apvts.getRawParameterValue("kickPitchEnd")->load();
    kick.pitchDecay = apvts.getRawParameterValue("kickPitchDecay")->load();
    kick.pitchCurve = apvts.getRawParameterValue("kickPitchCurve")->load();
    kick.bodyDecay = apvts.getRawParameterValue("kickBodyDecay")->load();
    kick.tail = apvts.getRawParameterValue("kickTail")->load();
    kick.click = apvts.getRawParameterValue("kickClick")->load();
    kick.clickTone = apvts.getRawParameterValue("kickClickTone")->load();
    kick.drive = apvts.getRawParameterValue("kickDrive")->load();
    kick.clip = apvts.getRawParameterValue("kickClip")->load();
    kick.transient = apvts.getRawParameterValue("kickTransient")->load();
    kick.sub = apvts.getRawParameterValue("kickSub")->load();
    kick.tune = apvts.getRawParameterValue("kickTune")->load();
    kick.phase = apvts.getRawParameterValue("kickPhase")->load();
    kick.outputLevel = apvts.getRawParameterValue("kickOutputLevel")->load();

    vstengine::match::BassRenderParams bass;
    bass.drive = apvts.getRawParameterValue("drive")->load();
    bass.release = apvts.getRawParameterValue("release")->load();
    bass.ampAttack = apvts.getRawParameterValue("ampAttack")->load();
    bass.ampDecay = apvts.getRawParameterValue("ampDecay")->load();
    bass.ampSustain = apvts.getRawParameterValue("ampSustain")->load();
    bass.filterCutoff = apvts.getRawParameterValue("filterCutoff")->load();
    bass.filterResonance = apvts.getRawParameterValue("filterResonance")->load();
    bass.filterDrive = apvts.getRawParameterValue("filterDrive")->load();
    bass.keyTracking = apvts.getRawParameterValue("keyTracking")->load();
    bass.pitchEnvAmount = apvts.getRawParameterValue("pitchEnvAmount")->load();
    bass.pitchEnvTime = apvts.getRawParameterValue("pitchEnvTime")->load();
    bass.pitchEnvCurve = apvts.getRawParameterValue("pitchEnvCurve")->load();
    bass.outputLevel = apvts.getRawParameterValue("outputLevel")->load();
    bass.midiNote = juce::jlimit(
        0, 127, static_cast<int>(
                    apvts.getRawParameterValue("rootNote")->load()));

    return vstengine::match::KickBassMatch::analyze(
        kick, bass, currentSampleRate);
}

void VstEngineAudioProcessor::applyMatchAdjustments(
    const vstengine::match::MatchAdjustments& adjustments)
{
    // Message-thread path (editor button). Every write clamps to the target
    // parameter's own numeric range, so nothing can leave the APVTS bounds.
    auto applyClamped = [this](const char* id, float plainValue) {
        if (auto* parameter = apvts.getParameter(id)) {
            const float normalized = juce::jlimit(
                0.0f, 1.0f, parameter->convertTo0to1(plainValue));
            parameter->setValue(normalized);
        }
    };

    if (auto* parameter = apvts.getParameter("kickTail")) {
        const float currentPlain = parameter->convertFrom0to1(
            parameter->getValue());
        applyClamped("kickTail",
                     currentPlain
                         * static_cast<float>(adjustments.kickTailMultiplier));
    }

    applyClamped("kickPhase", static_cast<float>(adjustments.kickPhaseDeg));

    if (auto* parameter = apvts.getParameter("outputLevel")) {
        const float currentPlain = parameter->convertFrom0to1(
            parameter->getValue());
        const double level = std::pow(10.0, adjustments.bassLevelDb / 20.0);
        applyClamped("outputLevel",
                     currentPlain * static_cast<float>(level));
    }

    applyClamped("matchBassTimingOffsetMs",
                 static_cast<float>(adjustments.bassTimingOffsetMs));
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

// addGeneratedMidi / addGeneratedMidiPpq / addGeneratedMidiFallback /
// flushGeneratedNoteState / triggerGeneratedNote moved verbatim into
// vstengine::midi::GeneratedNoteScheduler (libs/midi).

void VstEngineAudioProcessor::requestVoiceGlide(const int noteNumber,
                                                const float glideSeconds) noexcept
{
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::bass::PsyBassVoice*>(synth.getVoice(i)))
            voice->requestGlide(noteNumber, glideSeconds);
    }
}

void VstEngineAudioProcessor::clearVoiceGlideRequests() noexcept
{
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::bass::PsyBassVoice*>(synth.getVoice(i)))
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

    // --- Kick engine (issue #11 PHASE 5, exact parameter set) ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPitchStart", 1 }, "Kick Pitch Start",
        juce::NormalisableRange<float> { 0.0f, 36.0f, 0.1f }, 12.0f, "st"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPitchEnd", 1 }, "Kick Pitch End",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f, "st"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPitchDecay", 1 }, "Kick Pitch Decay",
        juce::NormalisableRange<float> { 0.001f, 0.5f, 0.001f, 0.4f },
        0.03f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPitchCurve", 1 }, "Kick Pitch Curve",
        juce::NormalisableRange<float> { 0.5f, 8.0f, 0.1f }, 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickBodyDecay", 1 }, "Kick Body Decay",
        juce::NormalisableRange<float> { 0.01f, 2.0f, 0.001f, 0.4f },
        0.16f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickTail", 1 }, "Kick Tail",
        juce::NormalisableRange<float> { 0.0f, 4.0f, 0.001f, 0.5f },
        0.3f, "s"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickClick", 1 }, "Kick Click",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickClickTone", 1 }, "Kick Click Tone",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickDrive", 1 }, "Kick Drive",
        juce::NormalisableRange<float> { 1.0f, 10.0f, 0.01f }, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickClip", 1 }, "Kick Clip",
        juce::NormalisableRange<float> { 0.1f, 1.0f, 0.005f }, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickTransient", 1 }, "Kick Transient",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickSub", 1 }, "Kick Sub",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickTune", 1 }, "Kick Tune",
        juce::NormalisableRange<float> { 24.0f, 48.0f, 1.0f }, 36.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPhase", 1 }, "Kick Phase",
        juce::NormalisableRange<float> { 0.0f, 360.0f, 1.0f }, 0.0f, "deg"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickOutputLevel", 1 }, "Kick Output Level",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.01f, 0.6f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "kickMidiChannel", 1 },
        "Kick MIDI Channel",
        1, 16, 2));

    // --- Kick/bass match (issue #11 PHASE 6) ---
    // User-visible bounded result of the match analysis: the bass's note
    // events are delayed by this many ms relative to the kick attack
    // (0 = no delay). Bound 16 ms by construction.
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "matchBassTimingOffsetMs", 1 },
        "Bass Timing Offset",
        0, 16, 0));

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
                sequenceData = vstengine::sequence::Sequence::deserialize(mb);
            }
        }
    }
    sequenceBridge.publish(sequenceData);
}

void VstEngineAudioProcessor::seedInitialSequence()
{
    sequenceData.clear();
    sequenceData.regenerateBySeed(0xD4A4u);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VstEngineAudioProcessor();
}
