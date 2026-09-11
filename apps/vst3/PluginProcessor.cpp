#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "midi/GeneratedNoteScheduler.h"
#include "midi/MidiExport.h"
#include "instrument/HostParameterSchema.h"
#include "modules/BuiltInProvider.h"
#include "preset/PresetManager.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
std::string slotParameterId(std::size_t slot, std::string_view suffix)
{
    char prefix[16] {};
    std::snprintf(prefix, sizeof(prefix), "slot%02zu", slot + 1);
    return std::string(prefix) + std::string(suffix);
}
} // namespace

VstEngineAudioProcessor::VstEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      rack(instrumentRegistry),
      presetStore(apvts)
{
    std::string diagnostic;
    if (!instrumentRegistry.registerProvider(
            vstengine::modules::createBuiltInProvider(), diagnostic))
        jassertfalse;

    auto initialRack = rack.state();
    initialRack[0].instrumentId = vstengine::modules::bassInstrumentId;
    initialRack[0].routing = { vstengine::rack::RouteMode::channel, 1,
                               0, 127, 1, 127, 0 };
    initialRack[0].macros = { 0.48f, 0.55f, 0.4f, 0.25f,
                              0.32f, 0.5f, 0.0f, 0.15f };
    initialRack[1].instrumentId = vstengine::modules::referenceInstrumentId;
    initialRack[1].routing = { vstengine::rack::RouteMode::channel, 2,
                               0, 127, 1, 127, 0 };
    initialRack[1].macros = { 0.5f, 0.65f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 0.0f };
    (void) rack.replaceState(initialRack, nullptr, diagnostic);
    cacheRackParameterPointers();
    for (std::size_t slot = 0; slot < rackControls.size(); ++slot)
        rackControls[slot].slotId = initialRack[slot].slotId;

    // Seed initial sequence with darkPsy pattern (canonical seeded operation
    // owned by the sequence module; same generator stream as before).
    seedInitialSequence();
    publishPartSequenceForAudio(0);
    (void) sequenceBridge.read(audioSequence);

    presetManager_ = std::make_unique<vstengine::PresetManager>(
        presetStore, parts[0].sequence,
        vstengine::PresetManager::FullStateCallbacks {
            [this] {
                syncRackControlsFromParameters();
                return vstengine::rack::state::serialize(
                    rack.snapshotState(rackControls));
            },
            [this](const juce::ValueTree& state) {
                std::array<vstengine::rack::PersistentSlotState,
                           vstengine::instrument::maxSlots> parsed;
                std::string diagnostic;
                return vstengine::rack::state::deserialize(state, parsed, diagnostic)
                    && rack.replaceState(parsed, nullptr, diagnostic);
            }
        });
}

void VstEngineAudioProcessor::prepareToPlay(const double sampleRate,
                                            const int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    (void) rack.prepare({ sampleRate,
        static_cast<std::uint32_t>(juce::jmax(1, samplesPerBlock)),
        static_cast<std::uint32_t>(juce::jmax(1, getTotalNumOutputChannels())) });
    setLatencySamples(static_cast<int>(rack.latencySamples()));

    // Reset the generated-playback scheduler and reseed the deterministic
    // probability RNG from the rngSeed parameter so the same project state
    // always reproduces the same probability decisions from the same playback
    // start point.
    const auto globalSeed = static_cast<std::uint32_t>(
        apvts.getRawParameterValue("rngSeed")->load());
    scheduler.reset(sampleRate);
    scheduler.reseedProbability(globalSeed ^ 0xB455A11u);

    // Preallocate the keyboard MIDI scratch buffer using JUCE's intended API.
    // Budget: 4096 bytes covers a dense block of keyboard events (256 events
    // at ~16 bytes/event sysex headroom) with safety margin. processBlock()
    // then only clears and reuses — no allocation or growth on the realtime
    // thread.
    keyboardScratch.ensureSize(32768);
    generatedScratch.ensureSize(65536);
    wasTransportPlaying = false;
}

void VstEngineAudioProcessor::releaseResources()
{
    rack.reset();
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
#if 0 // Fixed-Part implementation retained in history; generic Rack path below.
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int previousBassChannel = audioParts[0].midiChannel;
    syncPartControlsFromParameters(audioParts);
    audioParts[1].enabled = false;
    if (audioParts[0].midiChannel != previousBassChannel) {
        synth.allNotesOff(0, false);
        bassDelay.reset();
        scheduler.reset(currentSampleRate);
    }
    (void) sequenceBridge.read(audioSequence);

    // Explicit transport safety path, separate from ordinary note-off.
    if (auto* hostPlayHead = getPlayHead()) {
        if (const auto position = hostPlayHead->getPosition()) {
            const bool playing = position->getIsPlaying();
            if (wasTransportPlaying && !playing) {
                synth.allNotesOff(0, false);
                bassDelay.reset();
                scheduler.reset(currentSampleRate);
            }
            wasTransportPlaying = playing;
        }
    }

    // Invalidate glide requests from the previous block before new ones are
    // scheduled (same thread, same block ordering — no synchronization needed).
    clearVoiceGlideRequests();

    bassKeyboardScratch.clear();
    bassKeyboard.processNextMidiBuffer(
        bassKeyboardScratch, 0, buffer.getNumSamples(), true);
    auto trackAudition = [](const juce::MidiBuffer& events,
                            std::array<bool, 128>& notes) {
        for (const auto metadata : events) {
            const auto message = metadata.getMessage();
            if (message.isNoteOn(false))
                notes[static_cast<std::size_t>(message.getNoteNumber())] = true;
            else if (message.isNoteOff())
                notes[static_cast<std::size_t>(message.getNoteNumber())] = false;
            else if (message.isController()
                     && (message.getControllerNumber() == 120
                         || message.getControllerNumber() == 123))
                notes.fill(false);
        }
    };
    trackAudition(bassKeyboardScratch, bassAuditionNotes);

    // MIDI source isolation: the generator decision only sees EXTERNAL host
    // notes (note events present in this block's incoming buffer — computed
    // before anything is appended) and GUI keyboard notes. Internally
    // generated MIDI is never an input observation, so it can never suppress
    // generator playback in AUTO mode.
    const auto incomingHasNotes = containsNoteEvents(midi);
    const auto keyboardHasNotes = keyboardHasActiveNotes();
    const auto mode = currentMidiMode();
    const auto rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));

    // GENERATOR mode drops incoming host MIDI (only generated material plays).
    // GUI keyboard notes intentionally still work in GENERATOR mode, matching
    // the documented PIANO ROLL / GENERATOR / BOTH semantics.
    if (mode == MidiSourceMode::generator)
        midi.clear();

    if (panicRequested.exchange(false)) {
        midi.clear();
        synth.allNotesOff(0, false);
        bassDelay.reset();
        scheduler.reset(currentSampleRate);
        bassKeyboard.reset();
        bassKeyboardScratch.clear();
        bassAuditionNotes.fill(false);
    }

    // Authoritative host routing happens before internal sources are added.
    partRouter.route(midi, audioParts, partMidiBuffers);
    for (const auto metadata : partMidiBuffers[0])
        bassKeyboard.processNextMidiEvent(metadata.getMessage());

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

        const auto globalSeed = static_cast<std::uint32_t>(
            apvts.getRawParameterValue("rngSeed")->load());
        scheduler.process(partMidiBuffers[0], audioSequence, frame,
                          buffer.getNumSamples(), currentSampleRate,
                          audioParts[0].midiChannel, rootNote,
                          globalSeed ^ 0xB455A11u);
        for (int requestIndex = 0;
             requestIndex < scheduler.numGlideRequests(); ++requestIndex) {
            const auto request = scheduler.glideRequest(requestIndex);
            requestVoiceGlide(request.noteNumber, request.glideSeconds);
        }
        scheduler.clearGlideRequests();
    } else {
        scheduler.flush(partMidiBuffers[0], 0);
    }

    // Audition sources carry an explicit destination, independent of channel.
    for (const auto metadata : bassKeyboardScratch)
        (void) partRouter.routeToPart(metadata.getMessage(),
                                     metadata.samplePosition,
                                     vstengine::parts::PartId::bass,
                                     audioParts, partMidiBuffers);
    auto& bassMidi = partMidiBuffers[0];

    bassDelay.process(bassMidi, bassDelayScratch, buffer.getNumSamples(), 0);
    bassMidi.swapWith(bassDelayScratch);

    syncVoiceParameters();

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    bassAudioScratch.setSize(channels, samples, false, false, true);
    bassAudioScratch.clear();
    synth.renderNextBlock(bassAudioScratch, bassMidi, 0, samples);

    vstengine::parts::PartMixer::mix(
        audioParts, { &bassAudioScratch, nullptr }, buffer, samples);
#else
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    syncRackControlsFromParameters();
    (void) sequenceBridge.read(audioSequence);

    double bpm = 145.0;
    double ppq = 0.0;
    bool playing = true;
    bool havePpq = false;
    if (auto* host = getPlayHead())
        if (const auto position = host->getPosition()) {
            playing = position->getIsPlaying();
            if (const auto value = position->getBpm()) bpm = *value;
            if (const auto value = position->getPpqPosition()) {
                ppq = *value;
                havePpq = true;
            }
        }
    if (wasTransportPlaying && !playing) {
        rack.reset();
        scheduler.reset(currentSampleRate);
    }
    wasTransportPlaying = playing;

    keyboardScratch.clear();
    keyboard.processNextMidiBuffer(keyboardScratch, 0, buffer.getNumSamples(), true);
    for (const auto metadata : keyboardScratch) {
        const auto message = metadata.getMessage();
        if (message.isNoteOn(false))
            auditionNotes[static_cast<std::size_t>(message.getNoteNumber())] = true;
        else if (message.isNoteOff())
            auditionNotes[static_cast<std::size_t>(message.getNoteNumber())] = false;
    }

    const bool inputNotes = containsNoteEvents(midi);
    const auto mode = currentMidiMode();
    generatedScratch.clear();
    if (mode == MidiSourceMode::generator) midi.clear();
    const bool panicThisBlock = panicRequested.exchange(false);
    if (panicThisBlock) {
        midi.clear();
        keyboard.reset();
        keyboardScratch.clear();
        auditionNotes.fill(false);
        scheduler.reset(currentSampleRate);
        rack.reset();
    }

    if (!panicThisBlock && vstengine::midi::shouldRunGenerator(
            mode, inputNotes, keyboardHasActiveNotes())) {
        vstengine::midi::TransportFrame frame;
        frame.playing = playing;
        frame.havePpq = havePpq;
        frame.ppqAtBlockStart = ppq;
        frame.bpm = bpm;
        const auto root = juce::jlimit(0, 127,
            static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));
        const auto seed = static_cast<std::uint32_t>(
            apvts.getRawParameterValue("rngSeed")->load());
        const auto channel = rackControls[0].routing.channel;
        scheduler.process(generatedScratch, audioSequence, frame,
                          buffer.getNumSamples(),
                          currentSampleRate, channel, root, seed ^ 0xB455A11u);
        scheduler.clearGlideRequests();
    } else {
        scheduler.flush(generatedScratch, 0);
    }

    const auto blockSamples = static_cast<std::uint32_t>(buffer.getNumSamples());
    const auto hostCount = convertMidi(midi, hostEvents, blockSamples);
    const auto auditionCount = convertMidi(keyboardScratch, auditionEvents,
                                           blockSamples);
    const auto generatedCount = convertMidi(generatedScratch, generatedEvents,
                                             blockSamples);
    std::array<float*, 2> outputs {
        buffer.getWritePointer(0),
        buffer.getNumChannels() > 1 ? buffer.getWritePointer(1)
                                    : buffer.getWritePointer(0)
    };
    const auto channels = static_cast<std::size_t>(buffer.getNumChannels());
    const auto selected = selectedSlot.load();
    rack.process({ outputs.data(), channels },
        static_cast<std::uint32_t>(buffer.getNumSamples()),
        { hostEvents.data(), hostCount }, bpm, ppq, playing,
        { auditionEvents.data(), auditionCount }, rackControls[selected].slotId,
        rackControls, { generatedEvents.data(), generatedCount });
#endif
}

#if 0
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
#endif

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
    for (std::size_t note = 0; note < auditionNotes.size(); ++note) {
        if (auditionNotes[note])
            return true;
    }

    return false;
}

void VstEngineAudioProcessor::cacheRackParameterPointers()
{
    for (std::size_t slot = 0; slot < rackParameterRefs.size(); ++slot) {
        auto& refs = rackParameterRefs[slot];
        auto get = [this, slot](std::string_view suffix) {
            const auto id = slotParameterId(slot, suffix);
            return apvts.getRawParameterValue(id.c_str());
        };
        refs.midiIn = get("MidiIn"); refs.layer = get("Layer");
        refs.keyLow = get("KeyLow"); refs.keyHigh = get("KeyHigh");
        refs.velocityLow = get("VelocityLow");
        refs.velocityHigh = get("VelocityHigh"); refs.transpose = get("Transpose");
        refs.enabled = get("Enabled"); refs.mute = get("Mute");
        refs.solo = get("Solo"); refs.locked = get("Lock");
        refs.level = get("Level"); refs.pan = get("Pan");
        for (std::size_t macro = 0; macro < refs.macros.size(); ++macro)
            refs.macros[macro] = apvts.getRawParameterValue(
                vstengine::instrument::hostparams::macroId(slot, macro).c_str());
    }
}

void VstEngineAudioProcessor::syncRackControlsFromParameters() noexcept
{
    for (std::size_t slot = 0; slot < rackParameterRefs.size(); ++slot) {
        const auto& refs = rackParameterRefs[slot];
        const int midiIn = juce::jlimit(0, 16,
            static_cast<int>(std::lround(refs.midiIn->load())));
        vstengine::rack::Routing routing;
        routing.mode = midiIn == 0 ? vstengine::rack::RouteMode::off
            : refs.layer->load() >= 0.5f ? vstengine::rack::RouteMode::layer
                                        : vstengine::rack::RouteMode::channel;
        routing.channel = static_cast<std::uint8_t>(juce::jmax(1, midiIn));
        routing.keyLow = static_cast<std::uint8_t>(refs.keyLow->load());
        routing.keyHigh = static_cast<std::uint8_t>(refs.keyHigh->load());
        routing.velocityLow = static_cast<std::uint8_t>(refs.velocityLow->load());
        routing.velocityHigh = static_cast<std::uint8_t>(refs.velocityHigh->load());
        routing.transpose = static_cast<std::int8_t>(refs.transpose->load());
        auto& controls = rackControls[slot];
        controls.routing = routing;
        controls.enabled = refs.enabled->load() >= 0.5f;
        controls.mute = refs.mute->load() >= 0.5f;
        controls.solo = refs.solo->load() >= 0.5f;
        controls.locked = refs.locked->load() >= 0.5f;
        controls.level = refs.level->load();
        controls.pan = refs.pan->load();
        for (std::size_t macro = 0; macro < refs.macros.size(); ++macro)
            controls.macros[macro] = refs.macros[macro]->load();
    }
}

std::size_t VstEngineAudioProcessor::convertMidi(
    const juce::MidiBuffer& input,
    std::array<VoxMidiEventV1,
        vstengine::rack::RackRouter::eventCapacity>& output,
    std::uint32_t sampleCount) noexcept
{
    std::size_t count = 0;
    for (const auto metadata : input) {
        const auto message = metadata.getMessage();
        if (count >= output.size() || message.getRawDataSize() > 3
            || metadata.samplePosition < 0
            || static_cast<std::uint32_t>(metadata.samplePosition) >= sampleCount)
            continue;
        auto& event = output[count++];
        event.sampleOffset = static_cast<std::uint32_t>(juce::jmax(
            0, metadata.samplePosition));
        event.size = static_cast<std::uint8_t>(message.getRawDataSize());
        std::fill(std::begin(event.data), std::end(event.data),
                  static_cast<std::uint8_t>(0));
        std::copy_n(message.getRawData(), event.size, event.data);
    }
    return count;
}

void VstEngineAudioProcessor::syncPartControlsFromParameters(
    vstengine::parts::PartRegistry& destination) noexcept
{
    auto sync = [this](vstengine::parts::Part& part, const char* channel,
                       const char* mute, const char* solo, const char* lock,
                       const char* level, const char* pan) {
        part.midiChannel = juce::jlimit(
            1, 16, static_cast<int>(apvts.getRawParameterValue(channel)->load()));
        part.mute = apvts.getRawParameterValue(mute)->load() >= 0.5f;
        part.solo = apvts.getRawParameterValue(solo)->load() >= 0.5f;
        part.locked = apvts.getRawParameterValue(lock)->load() >= 0.5f;
        part.level = juce::jlimit(0.0f, 2.0f,
                                  apvts.getRawParameterValue(level)->load());
        part.pan = juce::jlimit(-1.0f, 1.0f,
                                apvts.getRawParameterValue(pan)->load());
    };
    sync(destination[0], "midiChannel", "bassMute", "bassSolo", "bassLock",
         "bassLevel", "bassPan");
    // Legacy Kick Part remains persistent migration data only. It is never
    // enabled or routed in active Vox Electronic Engine runtime.
    destination[1].enabled = false;
}

bool VstEngineAudioProcessor::isPartLocked(const int partIndex) const noexcept
{
    return partIndex != 0
        || apvts.getRawParameterValue("bassLock")->load() >= 0.5f;
}

void VstEngineAudioProcessor::syncParametersFromParts()
{
    auto setPlain = [this](const char* id, float plain) {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    };
    auto sync = [&setPlain](const vstengine::parts::Part& part,
                            const char* channel, const char* mute,
                            const char* solo, const char* lock,
                            const char* level, const char* pan) {
        setPlain(channel, static_cast<float>(part.midiChannel));
        setPlain(mute, part.mute ? 1.0f : 0.0f);
        setPlain(solo, part.solo ? 1.0f : 0.0f);
        setPlain(lock, part.locked ? 1.0f : 0.0f);
        setPlain(level, part.level);
        setPlain(pan, part.pan);
    };
    sync(parts[0], "midiChannel", "bassMute", "bassSolo", "bassLock",
         "bassLevel", "bassPan");
    juce::ignoreUnused(sync);
}

void VstEngineAudioProcessor::publishPartSequenceForAudio(const int partIndex) noexcept
{
    if (partIndex == 0)
        sequenceBridge.publish(parts[0].sequence);
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

#if 0
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
#endif

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
        "Bass MIDI Channel",
        1, 16, 1));

    for (auto parameter : {
             std::pair { "bassMute", "Bass Mute" },
             std::pair { "bassSolo", "Bass Solo" },
             std::pair { "bassLock", "Bass Lock" } })
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { parameter.first, 1 }, parameter.second, false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "bassLevel", 1 }, "Bass Part Level",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.01f }, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "bassPan", 1 }, "Bass Part Pan",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.01f }, 0.0f));

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

    for (auto parameter : {
             std::pair { "kickMute", "Kick Mute" },
             std::pair { "kickSolo", "Kick Solo" },
             std::pair { "kickLock", "Kick Lock" } })
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { parameter.first, 1 }, parameter.second, false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickLevel", 1 }, "Kick Part Level",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.01f }, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "kickPan", 1 }, "Kick Part Pan",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.01f }, 0.0f));

    // --- Kick/bass match (issue #11 PHASE 6) ---
    // User-visible bounded result of the match analysis: the bass's note
    // events are delayed by this many ms relative to the kick attack
    // (0 = no delay). Bound 16 ms by construction.
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "matchBassTimingOffsetMs", 1 },
        "Bass Timing Offset",
        0, 16, 0));

    // Stable host-facing automation bank. Module-specific parameters map to
    // these fixed IDs; loading another module never changes Cubase automation.
    juce::StringArray midiInputs { "Off" };
    for (int channel = 1; channel <= 16; ++channel)
        midiInputs.add("CH" + juce::String(channel));
    for (std::size_t slot = 0; slot < vstengine::instrument::maxSlots; ++slot) {
        const auto id = [slot](std::string_view suffix) {
            return juce::ParameterID {
                juce::String(slotParameterId(slot, suffix)), 1 };
        };
        const auto label = "Slot " + juce::String(static_cast<int>(slot + 1)) + " ";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            id("MidiIn"), label + "MIDI In", midiInputs,
            slot < 2 ? static_cast<int>(slot + 1) : 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            id("Layer"), label + "Layer", false));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            id("KeyLow"), label + "Key Low", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            id("KeyHigh"), label + "Key High", 0, 127, 127));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            id("VelocityLow"), label + "Velocity Low", 1, 127, 1));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            id("VelocityHigh"), label + "Velocity High", 1, 127, 127));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            id("Transpose"), label + "Transpose", -48, 48, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            id("Enabled"), label + "Enabled", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            id("Mute"), label + "Mute", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            id("Solo"), label + "Solo", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            id("Lock"), label + "Lock", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            id("Level"), label + "Level",
            juce::NormalisableRange<float> { 0.0f, 2.0f, 0.001f }, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            id("Pan"), label + "Pan",
            juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f));
        for (std::size_t macro = 0;
             macro < vstengine::instrument::macrosPerSlot; ++macro)
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID {
                    vstengine::instrument::hostparams::macroId(slot, macro), 1 },
                vstengine::instrument::hostparams::macroName(slot, macro),
                juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f }, 0.5f));
    }

    return { params.begin(), params.end() };
}

bool VstEngineAudioProcessor::loadSlotInstrument(
    std::size_t slot, std::string_view instrumentId, juce::String& diagnostic)
{
    suspendProcessing(true);
    std::string detail;
    const bool accepted = rack.loadModule(slot, instrumentId, nullptr, detail);
    suspendProcessing(false);
    diagnostic = detail;
    return accepted;
}

int VstEngineAudioProcessor::nextFreeChannel() const noexcept
{
    std::array<bool, 17> used {};
    for (const auto& refs : rackParameterRefs) {
        const int channel = juce::jlimit(0, 16,
            static_cast<int>(std::lround(refs.midiIn->load())));
        if (channel > 0) used[static_cast<std::size_t>(channel)] = true;
    }
    for (int channel = 1; channel <= 16; ++channel)
        if (!used[static_cast<std::size_t>(channel)]) return channel;
    return 0;
}

bool VstEngineAudioProcessor::assignSlotChannel(
    std::size_t slot, int channel, ChannelConflictAction action,
    juce::String& diagnostic)
{
    if (slot >= rackParameterRefs.size() || channel < 0 || channel > 16) {
        diagnostic = "Invalid slot/channel";
        return false;
    }
    auto setPlain = [this](const std::string& id, float value) {
        if (auto* parameter = apvts.getParameter(id.c_str())) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    std::size_t conflict = rackParameterRefs.size();
    if (channel > 0)
        for (std::size_t i = 0; i < rackParameterRefs.size(); ++i)
            if (i != slot && rackParameterRefs[i].enabled->load() >= 0.5f
                && rackParameterRefs[i].layer->load() < 0.5f
                && static_cast<int>(std::lround(
                    rackParameterRefs[i].midiIn->load())) == channel) {
                conflict = i;
                break;
            }
    if (conflict < rackParameterRefs.size()) {
        if (action == ChannelConflictAction::reject) {
            diagnostic = "Channel occupied; choose SWAP, MOVE, or LAYER";
            return false;
        }
        if (action == ChannelConflictAction::swap) {
            const float previous = rackParameterRefs[slot].midiIn->load();
            setPlain(slotParameterId(conflict, "MidiIn"), previous);
        } else if (action == ChannelConflictAction::move) {
            setPlain(slotParameterId(conflict, "MidiIn"), 0.0f);
        } else if (action == ChannelConflictAction::layer) {
            setPlain(slotParameterId(slot, "Layer"), 1.0f);
        }
    } else if (action != ChannelConflictAction::layer) {
        setPlain(slotParameterId(slot, "Layer"), 0.0f);
    }
    setPlain(slotParameterId(slot, "MidiIn"), static_cast<float>(channel));
    diagnostic.clear();
    return true;
}


juce::File VstEngineAudioProcessor::createPartMidiFile(const int partIndex)
{
    // Export goes through the shared MIDI export builder so timing mode,
    // ratchet and slide behave exactly as documented and identically to
    // realtime playback.
    vstengine::midiexport::Options options;
    if (partIndex != 0)
        return {};
    syncPartControlsFromParameters(parts);
    constexpr std::size_t index = 0;
    options.channel = parts[index].midiChannel;
    options.rootNote = juce::jlimit(
        0, 127,
        static_cast<int>(apvts.getRawParameterValue("rootNote")->load()));
    options.seed = static_cast<std::uint32_t>(
        apvts.getRawParameterValue("rngSeed")->load())
        ^ 0xB455A11u;

    auto exportSequence = parts[index].sequence;
    const auto track =
        vstengine::midiexport::buildSequenceTrack(exportSequence, options);

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(
        static_cast<int>(options.ticksPerQuarter));
    midiFile.addTrack(track);

    const auto tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("Vox-Trance-Engine-Bass.mid");

    tempFile.deleteFile();

    if (auto stream = tempFile.createOutputStream()) {
        if (midiFile.writeTo(*stream))
            return tempFile;
    }

    return {};
}

void VstEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    syncRackControlsFromParameters();
    auto tree = apvts.copyState();
    for (;;) {
        const auto stale = tree.getChildWithName("PARTS");
        if (!stale.isValid())
            break;
        tree.removeChild(stale, nullptr);
    }
    for (;;) {
        const auto stale = tree.getChildWithName("RACK");
        if (!stale.isValid()) break;
        tree.removeChild(stale, nullptr);
    }
    tree.setProperty("stateSchemaVersion", 3, nullptr);
    tree.addChild(vstengine::rack::state::serialize(
        rack.snapshotState(rackControls)), -1, nullptr);
    juce::MemoryBlock sequenceData;
    parts[0].sequence.serialize(sequenceData);
    tree.setProperty("sequenceData", sequenceData.toBase64Encoding(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(tree.createXml());
    copyXmlToBinary(*xml, destData);
}

void VstEngineAudioProcessor::setStateInformation(const void* data,
                                                    const int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        const auto rackTree = tree.getChildWithName("RACK");
        std::array<vstengine::rack::PersistentSlotState,
                   vstengine::instrument::maxSlots> parsedRack;
        std::string rackDiagnostic;
        if (rackTree.isValid()) {
            if (!vstengine::rack::state::deserialize(
                    rackTree, parsedRack, rackDiagnostic))
                return;
        }
        const auto partState = tree.getChildWithName("PARTS");
        const bool restoredParts =
            vstengine::parts::PartState::restore(partState, parts);
        auto apvtsState = tree.createCopy();
        for (;;) {
            const auto extra = apvtsState.getChildWithName("PARTS");
            if (!extra.isValid())
                break;
            apvtsState.removeChild(extra, nullptr);
        }
        for (;;) {
            const auto extra = apvtsState.getChildWithName("RACK");
            if (!extra.isValid()) break;
            apvtsState.removeChild(extra, nullptr);
        }
        const auto previousApvts = apvts.copyState();
        apvts.replaceState(apvtsState);
        if (rackTree.isValid()
            && !rack.replaceState(parsedRack, nullptr, rackDiagnostic)) {
            apvts.replaceState(previousApvts);
            return;
        }
        if (rackTree.isValid())
            for (std::size_t slot = 0; slot < rackControls.size(); ++slot)
                rackControls[slot].slotId = parsedRack[slot].slotId;

        if (!rackTree.isValid() && restoredParts) {
            // Keep old Kick data readable, but never reactivate removed product
            // content. Map it to explicit unresolved Rack state.
            parts[1].enabled = false;
            syncParametersFromParts();
            auto migrated = rack.state();
            migrated[0].instrumentId = vstengine::modules::bassInstrumentId;
            migrated[0].routing.mode = vstengine::rack::RouteMode::channel;
            migrated[0].routing.channel = static_cast<std::uint8_t>(parts[0].midiChannel);
            migrated[0].mute = parts[0].mute; migrated[0].solo = parts[0].solo;
            migrated[0].locked = parts[0].locked; migrated[0].level = parts[0].level;
            migrated[0].pan = parts[0].pan;
            migrated[1].instrumentId = "com.ultimavox.legacy-kick";
            migrated[1].routing.mode = vstengine::rack::RouteMode::off;
            migrated[1].routing.channel = static_cast<std::uint8_t>(parts[1].midiChannel);
            migrated[1].enabled = false;
            if (auto legacyXml = partState.createXml()) {
                const auto text = legacyXml->toString().toStdString();
                migrated[1].modulePayload.resize(text.size());
                std::memcpy(migrated[1].modulePayload.data(), text.data(), text.size());
            }
            std::string diagnostic;
            (void) rack.replaceState(migrated, nullptr, diagnostic);
            for (std::size_t slot = 0; slot < rackControls.size(); ++slot)
                rackControls[slot].slotId = migrated[slot].slotId;
            auto setPlain = [this](const std::string& id, float value) {
                if (auto* parameter = apvts.getParameter(id.c_str()))
                    parameter->setValueNotifyingHost(
                        parameter->convertTo0to1(value));
            };
            setPlain(slotParameterId(0, "MidiIn"),
                     static_cast<float>(parts[0].midiChannel));
            setPlain(slotParameterId(0, "Mute"), parts[0].mute ? 1.0f : 0.0f);
            setPlain(slotParameterId(0, "Solo"), parts[0].solo ? 1.0f : 0.0f);
            setPlain(slotParameterId(0, "Lock"), parts[0].locked ? 1.0f : 0.0f);
            setPlain(slotParameterId(0, "Level"), parts[0].level);
            setPlain(slotParameterId(0, "Pan"), parts[0].pan);
            setPlain(slotParameterId(1, "MidiIn"), 0.0f);
            setPlain(slotParameterId(1, "Enabled"), 0.0f);
            const std::array<float, 8> migratedMacros {
                apvts.getRawParameterValue("filterCutoff")->load(),
                apvts.getRawParameterValue("filterResonance")->load(),
                apvts.getRawParameterValue("pitchEnvAmount")->load() / 36.0f,
                juce::jlimit(0.0f, 1.0f,
                    (apvts.getRawParameterValue("ampDecay")->load() - 0.015f)
                    / 0.18f),
                juce::jlimit(0.0f, 1.0f,
                    (apvts.getRawParameterValue("drive")->load() - 1.0f) / 5.0f),
                apvts.getRawParameterValue("ampSustain")->load(), 0.0f,
                apvts.getRawParameterValue("keyTracking")->load()
            };
            for (std::size_t macro = 0; macro < migratedMacros.size(); ++macro)
                setPlain(vstengine::instrument::hostparams::macroId(0, macro),
                         migratedMacros[macro]);
        } else if (!rackTree.isValid()) {
            // PR #18 migration: APVTS routing + legacy Sequence -> Bass Part;
            // Kick keeps canonical defaults and independent empty sequence.
            syncPartControlsFromParameters(parts);
        }
        const auto seqBase64 = tree.getProperty("sequenceData").toString();
        juce::MemoryBlock sequenceData;
        if (sequenceData.fromBase64Encoding(seqBase64)
            && vstengine::sequence::Sequence::isValidSerialization(sequenceData))
            parts[0].sequence =
                vstengine::sequence::Sequence::deserialize(sequenceData);
    }
    publishPartSequenceForAudio(0);
}

void VstEngineAudioProcessor::seedInitialSequence()
{
    parts[0].sequence.clear();
    parts[0].sequence.regenerateBySeed(0xD4A4u);
    parts[1].sequence.clear();
    parts[1].enabled = false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VstEngineAudioProcessor();
}
