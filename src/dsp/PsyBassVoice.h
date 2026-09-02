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
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&,
                         int startSample, int numSamples) override;

    // Drive/distortion
    void setDrive(float value) noexcept { drive = value; }

    // ADSR (release overrides if voice not playing)
    void setAmpRelease(float seconds) noexcept { ampRelease = seconds; }

    // Pitch envelope
    void setPitchEnvelopeAmount(float v) noexcept { pitchEnvelopeAmount = v; }
    void setPitchEnvelopeTime(float v) noexcept { pitchEnvelopeTimeSeconds = v; }
    void setPitchEnvelopeCurve(float v) noexcept { pitchEnvelopeCurve = v; }

    // ADSR per-parameter
    void setAmpAttack(float v) noexcept { ampAttack = v; }
    void setAmpDecay(float v) noexcept { ampDecay = v; }
    void setAmpSustain(float v) noexcept { ampSustain = v; }

    // Filter
    void setFilterCutoff(float v) noexcept { filterCutoff = v; }
    void setFilterResonance(float v) noexcept { filterResonance = v; }
    void setFilterDrive(float v) noexcept { filterDrive = v; }
    void setKeyTracking(float v) noexcept { keyTracking = v; }

    // Output
    void setOutputLevel(float v) noexcept { outputLevel = v; }

private:
    // Oscillator
    double phase {};
    double baseFrequency {};

    // Amplitude
    float level {};
    float ampRelease { 0.035f };
    float ampAttack { 0.001f };
    float ampDecay { 0.055f };
    float ampSustain { 0.72f };
    juce::ADSR ampEnvelope;

    // Pitch envelope
    float pitchEnvelopeAmount { 12.0f };  // semitones
    float pitchEnvelopeTime { 0.018f };   // seconds
    float pitchEnvelopeCurve { 2.0f };    // exponent
    int pitchEnvelopeSamples {};
    int pitchEnvelopePosition {};

    // Filter
    float filterCutoff { 0.5f };          // normalized 0-1
    float filterResonance { 0.7f };       // 0-1 → Q
    float filterDrive { 1.0f };           // pre-filter drive
    float keyTracking { 0.0f };           // 0=no, 1.0=full
    juce::dsp::IIR::Filter<float> filter;

    // Distortion + output
    float drive { 1.8f };
    float outputLevel { 1.0f };

    void rebuildFilter(double sampleRate, int midiNoteNumber);
};
} // namespace vstengine::dsp
