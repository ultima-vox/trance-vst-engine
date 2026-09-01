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
    void setPitchEnvelope(float semitones, float seconds) noexcept;
    void setAmpEnvelope(float attack, float decay, float sustain, float release) noexcept;
    void setFilter(float cutoffHz, float resonanceQ);

private:
    void updateAmpEnvelope();

    double phase {};
    double baseFrequency {};
    float level {};
    float drive { 1.8f };

    float pitchEnvSemitones { 12.0f };
    float pitchEnvSeconds { 0.018f };

    float attackSeconds { 0.001f };
    float decaySeconds { 0.055f };
    float sustainLevel { 0.72f };
    float releaseSeconds { 0.035f };

    float cutoffHz { 1800.0f };
    float resonanceQ { 0.8f };

    int pitchEnvelopeSamples {};
    int pitchEnvelopePosition {};

    juce::ADSR ampEnvelope;
    juce::IIRFilter filter;
};

} // namespace vstengine::dsp
