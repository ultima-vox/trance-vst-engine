#pragma once
#include <JuceHeader.h>
#include <memory>
#include "dsp/PsyBassVoice.h"
#include "generator/Sequence.h"
#include "preset/PresetManager.h"

class VstEngineAudioProcessor final : public juce::AudioProcessor {
public:
    enum class MidiSourceMode : int {
        autoDetect = 0,
        pianoRoll,
        generator,
        both
    };

    // Parameters that make up the psy-bass engine "sound". Sound presets store
    // exactly these; global MIDI/generator settings (midiMode, midiChannel,
    // rootNote, rngSeed) and the sequence belong to Full presets only.
    static constexpr const char* soundParameterIds[] = {
        "drive", "release", "ampAttack", "ampDecay", "ampSustain",
        "filterCutoff", "filterResonance", "filterDrive", "keyTracking",
        "pitchEnvAmount", "pitchEnvTime", "pitchEnvCurve", "outputLevel"
    };
    static constexpr int numSoundParameterIds = 13;

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
    vstengine::generator::Sequence& sequence() noexcept { return sequenceData; }
    vstengine::PresetManager* presetManager() noexcept { return presetManager_.get(); }
    int getCurrentPlayHeadStep() const noexcept { return playHeadStep; }
    juce::File createGeneratedMidiFile();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void addGeneratedMidi(juce::MidiBuffer& midi, int numSamples, int channel, int rootNote);
    void syncVoiceParameters();
    void seedInitialSequence();
    void resetPlaybackState();
    void triggerGeneratedNote(juce::MidiBuffer& midi, int offset, int channel,
                              int rootNote, const vstengine::generator::Step& step,
                              double samplesPerStep, double subNoteSamples);
    void requestVoiceGlide(int noteNumber, float glideSeconds) noexcept;
    void clearVoiceGlideRequests() noexcept;
    static bool containsNoteEvents(const juce::MidiBuffer& midi) noexcept;
    MidiSourceMode currentMidiMode() const noexcept;
    bool keyboardHasActiveNotes() const noexcept;

    juce::Synthesiser synth;
    juce::MidiKeyboardState midiKeyboardState;
    juce::AudioProcessorValueTreeState apvts;
    vstengine::generator::Sequence sequenceData;
    std::unique_ptr<vstengine::PresetManager> presetManager_;

    // Generated-playback state (audio thread only, all preallocated)
    int currentStep {};
    int playHeadStep {-1};
    int heldNote { -1 };
    int heldChannel { 1 };
    double samplesUntilNextStep {};
    double samplesUntilNoteOff { -1.0 };
    int ratchetsRemaining {};
    double samplesUntilNextRatchet {};
    double subNoteDuration {};
    int lastGeneratedNote { -1 };
    vstengine::generator::ProbabilityState probabilityState {};
    std::uint64_t playedStepCounter {};
    bool transportWasPlaying { false };
    double currentSampleRate { 44100.0 };

    // Preallocated scratch buffer for keyboard MIDI. processBlock() reuses it
    // every block (clear, processNextMidiBuffer, merge) so the realtime
    // callback never constructs a local MidiBuffer or grows one past this
    // reserved capacity. Capacity is reserved once in prepareToPlay.
    juce::MidiBuffer keyboardMidiScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessor)
};
