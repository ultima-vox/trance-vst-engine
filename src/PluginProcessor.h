#pragma once
#include <JuceHeader.h>
#include "dsp/PsyBassVoice.h"
#include "generator/PatternGenerator.h"

class VstEngineAudioProcessor final : public juce::AudioProcessor {
public:
    enum class MidiSourceMode : int {
        autoDetect = 0,
        pianoRoll,
        generator,
        both
    };

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
    juce::File createGeneratedMidiFile();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void addGeneratedMidi(juce::MidiBuffer& midi, int numSamples, int channel, int rootNote);
    static bool containsNoteEvents(const juce::MidiBuffer& midi) noexcept;
    MidiSourceMode currentMidiMode() const noexcept;
    bool keyboardHasActiveNotes() const noexcept;

    juce::Synthesiser synth;
    juce::MidiKeyboardState midiKeyboardState;
    juce::AudioProcessorValueTreeState apvts;
    vstengine::generator::Pattern pattern;

    int currentStep {};
    int heldNote { -1 };
    int heldChannel { 1 };
    double samplesUntilNextStep {};
    double samplesUntilNoteOff { -1.0 };
    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessor)
};
