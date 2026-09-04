#pragma once
#include <JuceHeader.h>
#include <memory>
#include "dsp/PsyBassVoice.h"
#include "generator/Sequence.h"
#include "midi/SourceSelector.h"
#include "preset/PresetManager.h"

class VstEngineAudioProcessor final : public juce::AudioProcessor {
public:
    // Canonical MIDI source mode (AUTO / PIANO ROLL / GENERATOR / BOTH).
    // The decision rule lives in vstengine::midi::shouldRunGenerator() so the
    // full mode x input-presence matrix is unit-tested in isolation.
    using MidiSourceMode = vstengine::midi::SourceMode;

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
    // Host-PPQ-synchronized playback: schedules step onsets from the host's
    // quarter-note position so transport start/seek/loop always land on the
    // correct sequence step. Used when the playhead provides PPQ.
    void addGeneratedMidiPpq(juce::MidiBuffer& midi, int numSamples, int channel,
                             int rootNote, double ppqAtBlockStart, double bpm);
    // Free-running fallback (BPM-derived step timing) used when the host does
    // not provide PPQ. Documented + tested: keeps the historical behavior.
    void addGeneratedMidiFallback(juce::MidiBuffer& midi, int numSamples,
                                  int channel, int rootNote, double bpm);
    // Emits a note-off for any sounding generated note and resets all
    // generated-playback state. Called on stop, mode switches, seek/loop
    // realign and flush paths so generated notes can never get stuck.
    void flushGeneratedNoteState(juce::MidiBuffer& midi, int offset) noexcept;
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
    // Host-PPQ continuity state (audio thread only). continuityValid becomes
    // true after the first block whose end PPQ was observed; a PPQ position
    // that does not continue from the previous block's end is a seek or
    // cycle-loop jump and forces a hard realign of the generated cursor.
    // The jump tolerance is computed per block as half a step of the current
    // canonical grid (see addGeneratedMidiPpq), which is far below any real
    // seek/loop distance yet far above host rounding / smooth tempo-drift.
    bool continuityValid { false };
    double lastBlockEndPpq {};
    double currentSampleRate { 44100.0 };

    // Preallocated scratch buffer for keyboard MIDI. processBlock() reuses it
    // every block (clear, processNextMidiBuffer, merge) so the realtime
    // callback never constructs a local MidiBuffer or grows one past this
    // reserved capacity. Capacity is reserved once in prepareToPlay.
    juce::MidiBuffer keyboardMidiScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessor)
};
