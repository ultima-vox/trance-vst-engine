#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include "bass/PsyBassVoice.h"
#include "midi/GeneratedNoteScheduler.h"
#include "midi/SourceSelector.h"
#include "parts/PartMidiDelay.h"
#include "parts/PartMixer.h"
#include "parts/PartRouter.h"
#include "parts/PartState.h"
#include "preset/PresetManager.h"
#include "sequence/Sequence.h"
#include "sequence/RealtimeSequenceBridge.h"

class VstEngineAudioProcessor final : public juce::AudioProcessor {
public:
    // Canonical MIDI source mode (AUTO / PIANO ROLL / GENERATOR / BOTH).
    // The decision rule lives in vstengine::midi::shouldRunGenerator() so the
    // full mode x input-presence matrix is unit-tested in isolation.
    using MidiSourceMode = vstengine::midi::SourceMode;

    VstEngineAudioProcessor();
    ~VstEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts; }
    juce::MidiKeyboardState& bassKeyboardState() noexcept { return bassKeyboard; }
    juce::MidiKeyboardState& keyboardState() noexcept { return bassKeyboard; }
    vstengine::sequence::Sequence& partSequence(int partIndex) noexcept
    {
        return parts[static_cast<std::size_t>(partIndex == 0 ? 0 : 1)].sequence;
    }
    vstengine::sequence::Sequence& sequence() noexcept { return partSequence(0); }
    vstengine::PresetManager* presetManager() noexcept { return presetManager_.get(); }
    int getPartPlayHeadStep(int partIndex) const noexcept
    {
        return partIndex == 0 ? scheduler.playHeadStep() : -1;
    }
    int getCurrentPlayHeadStep() const noexcept { return getPartPlayHeadStep(0); }
    void requestPanic() noexcept { panicRequested.store(true); }
    void publishPartSequenceForAudio(int partIndex) noexcept;
    void publishSequenceForAudio() noexcept { publishPartSequenceForAudio(0); }
    juce::File createPartMidiFile(int partIndex);
    juce::File createGeneratedMidiFile() { return createPartMidiFile(0); }
    [[nodiscard]] vstengine::parts::PartRegistry& partRegistry() noexcept
    {
        return parts;
    }
    [[nodiscard]] bool isPartLocked(int partIndex) const noexcept;

private:
    // APVTS bridge passed to the preset module (vstengine::PresetStateStore).
    class ApvtsPresetStore final : public vstengine::PresetStateStore {
    public:
        explicit ApvtsPresetStore(juce::AudioProcessorValueTreeState& s)
            : apvts(s) {}
        [[nodiscard]] juce::ValueTree copyState() const override
        {
            return apvts.copyState();
        }
        void replaceState(juce::ValueTree newState) override
        {
            apvts.replaceState(std::move(newState));
        }
        [[nodiscard]] float convertTo0to1(const char* parameterId,
                                          float plainValue) const override
        {
            auto* ranged = apvts.getParameter(parameterId);
            return ranged != nullptr ? ranged->convertTo0to1(plainValue)
                                     : plainValue;
        }

    private:
        juce::AudioProcessorValueTreeState& apvts;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncVoiceParameters();
    void seedInitialSequence();
    void requestVoiceGlide(int noteNumber, float glideSeconds) noexcept;
    void clearVoiceGlideRequests() noexcept;
    static bool containsNoteEvents(const juce::MidiBuffer& midi) noexcept;
    MidiSourceMode currentMidiMode() const noexcept;
    bool keyboardHasActiveNotes() const noexcept;
    void syncPartControlsFromParameters(
        vstengine::parts::PartRegistry& destination) noexcept;
    void syncParametersFromParts();

    juce::Synthesiser synth;
    juce::MidiKeyboardState bassKeyboard;
    juce::AudioProcessorValueTreeState apvts;
    vstengine::parts::PartRegistry parts;
    vstengine::parts::PartRegistry audioParts;
    vstengine::sequence::RealtimeSequenceBridge sequenceBridge;
    vstengine::sequence::Sequence audioSequence { 16 };
    ApvtsPresetStore presetStore;
    std::unique_ptr<vstengine::PresetManager> presetManager_;
    // Generated-playback state machine (audio thread, see libs/midi).
    vstengine::midi::GeneratedNoteScheduler scheduler;
    vstengine::parts::PartRouter partRouter;
    vstengine::parts::PartMidiBuffers partMidiBuffers;
    // Existing bounded delay primitive, now applied after authoritative routing
    // to Bass buffer only. It can never expose another channel to Bass DSP.
    vstengine::parts::PartMidiDelay bassDelay;
    double currentSampleRate { 44100.0 };
    bool wasTransportPlaying { false };
    std::atomic<bool> panicRequested { false };

    // Preallocated scratch buffer for keyboard MIDI. processBlock() reuses it
    // every block (clear, processNextMidiBuffer, merge) so the realtime
    // callback never constructs a local MidiBuffer or grows one past this
    // reserved capacity. Capacity is reserved once in prepareToPlay.
    juce::MidiBuffer bassKeyboardScratch;
    std::array<bool, 128> bassAuditionNotes {};
    // Preallocated scratch for delayed Bass MIDI.
    juce::MidiBuffer bassDelayScratch;
    juce::AudioBuffer<float> bassAudioScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessor)
};
