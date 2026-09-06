#pragma once
#include <JuceHeader.h>
#include <memory>
#include "bass/PsyBassVoice.h"
#include "midi/GeneratedNoteScheduler.h"
#include "midi/SourceSelector.h"
#include "preset/PresetManager.h"
#include "sequence/Sequence.h"

class VstEngineAudioProcessor final : public juce::AudioProcessor {
public:
    // Canonical MIDI source mode (AUTO / PIANO ROLL / GENERATOR / BOTH).
    // The decision rule lives in vstengine::midi::shouldRunGenerator() so the
    // full mode x input-presence matrix is unit-tested in isolation.
    using MidiSourceMode = vstengine::midi::SourceMode;

    VstEngineAudioProcessor();
    ~VstEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts; }
    juce::MidiKeyboardState& keyboardState() noexcept { return midiKeyboardState; }
    vstengine::sequence::Sequence& sequence() noexcept { return sequenceData; }
    vstengine::PresetManager* presetManager() noexcept { return presetManager_.get(); }
    int getCurrentPlayHeadStep() const noexcept { return scheduler.playHeadStep(); }
    juce::File createGeneratedMidiFile();

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

    juce::Synthesiser synth;
    juce::MidiKeyboardState midiKeyboardState;
    juce::AudioProcessorValueTreeState apvts;
    vstengine::sequence::Sequence sequenceData;
    ApvtsPresetStore presetStore;
    std::unique_ptr<vstengine::PresetManager> presetManager_;
    // Generated-playback state machine (audio thread, see libs/midi).
    vstengine::midi::GeneratedNoteScheduler scheduler;
    double currentSampleRate { 44100.0 };

    // Preallocated scratch buffer for keyboard MIDI. processBlock() reuses it
    // every block (clear, processNextMidiBuffer, merge) so the realtime
    // callback never constructs a local MidiBuffer or grows one past this
    // reserved capacity. Capacity is reserved once in prepareToPlay.
    juce::MidiBuffer keyboardMidiScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessor)
};
