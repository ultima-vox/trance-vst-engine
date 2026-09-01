#pragma once
#include <JuceHeader.h>

namespace vstengine::dsp {
class PsyBassSound final : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class PsyBassVoice final : public juce::SynthesiserVoice {
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

    void setDrive(float value) noexcept { drive = value; }
    void setRelease(float seconds) noexcept { releaseSeconds = seconds; }

private:
    double phase {};
    double baseFrequency {};
    float level {};
    float drive { 1.8f };
    float releaseSeconds { 0.035f };
    int pitchEnvelopeSamples {};
    int pitchEnvelopePosition {};
    juce::ADSR ampEnvelope;
};
} // namespace vstengine::dsp
