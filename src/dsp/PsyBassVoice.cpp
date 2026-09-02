#include "PsyBassVoice.h"
#include <cmath>

namespace vstengine::dsp {
bool PsyBassVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PsyBassSound*>(sound) != nullptr;
}

void PsyBassVoice::rebuildFilter(double sampleRate, int midiNoteNumber)
{
    // Map normalized cutoff [0,1] to Hz [20, 18000]
    const auto normCutoff = juce::jlimit(0.0f, 1.0f, filterCutoff);
    const auto baseCutoffHz = juce::jmap(normCutoff, 20.0f, 18000.0f);

    // Key tracking: higher notes → higher cutoff
    // Reference note = A2 (MIDI 45) = ~110Hz
    const float keyOffset = keyTracking * static_cast<float>(midiNoteNumber - 45);
    const auto cutoffHz = juce::jlimit(20.0, 18000.0,
                                      baseCutoffHz * (1.0f + keyOffset * 0.05f));

    // Resonance → Q: 0→0.5, 1→3.0
    const auto Q = juce::jmap(filterResonance, 0.5f, 3.0f);

    auto coeffs = juce::dsp::FilterDesign::lowPassToCoeffs<float>(
        juce::dsp::InterpolationMethod::bilinear,
        sampleRate, cutoffHz, Q);

    filter.setCoefficients(std::move(coeffs));
}

void PsyBassVoice::startNote(const int midiNoteNumber, const float velocity,
                              juce::SynthesiserSound*, const int)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    phase = 0.0;
    level = velocity;

    // Rebuild filter with key tracking
    rebuildFilter(getSampleRate(), midiNoteNumber);

    // Pitch envelope
    pitchEnvelopePosition = 0;
    pitchEnvelopeSamples = juce::jmax(1,
        static_cast<int>(pitchEnvelopeTime * getSampleRate()));

    // ADSR
    juce::ADSR::Parameters p;
    p.attack = ampAttack;
    p.decay = ampDecay;
    p.sustain = ampSustain;
    p.release = ampRelease;
    ampEnvelope.setSampleRate(getSampleRate());
    ampEnvelope.setParameters(p);
    ampEnvelope.noteOn();
}

void PsyBassVoice::stopNote(const float, const bool allowTailOff)
{
    if (allowTailOff) {
        ampEnvelope.noteOff();
    } else {
        ampEnvelope.reset();
        clearCurrentNote();
    }
}

void PsyBassVoice::renderNextBlock(juce::AudioBuffer<float>& output,
                                    const int startSample, const int numSamples)
{
    if (!isVoiceActive())
        return;

    // Gain compensation: reduce pre-distortion gain as drive increases
    // Prevents output level spike from tanh saturation
    const float driveGainComp = 1.0f / (0.5f + drive * 0.15f);

    // Filter drive
    const float fd = juce::jlimit(0.0f, 5.0f, filterDrive);

    for (int sample = 0; sample < numSamples; ++sample) {
        // --- Pitch envelope ---
        const auto t = juce::jlimit(0.0, 1.0,
            static_cast<double>(pitchEnvelopePosition) /
                static_cast<double>(pitchEnvelopeSamples));
        const auto semitones = pitchEnvelopeAmount
            * std::pow(1.0 - t, pitchEnvelopeCurve);
        const auto frequency = baseFrequency
            * std::pow(2.0, semitones / 12.0);

        // --- Oscillator: fundamental + 2nd harmonic ---
        const auto fundamental = static_cast<float>(std::sin(phase));
        const auto harmonic = static_cast<float>(0.20 * std::sin(phase * 2.0));
        const auto raw = (fundamental + harmonic) * level;

        // --- Amp envelope ---
        const auto envLevel = ampEnvelope.getNextSample();

        // --- Distortion ---
        const auto shaped = std::tanh(raw * envLevel * drive * driveGainComp)
            * 0.55f;

        // --- Filter drive ---
        const auto driven = shaped * fd;

        // --- Resonant low-pass filter ---
        const auto filtered = filter.processSample(driven);

        // --- Output level ---
        const auto out = filtered * outputLevel;

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addSample(ch, startSample + sample, out);

        // --- Advance phase ---
        const auto phaseDelta = juce::MathConstants<double>::twoPi
            * frequency / getSampleRate();
        phase += phaseDelta;
        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        // --- Advance pitch envelope ---
        ++pitchEnvelopePosition;

        // --- Check voice active ---
        if (!ampEnvelope.isActive()) {
            clearCurrentNote();
            break;
        }
    }
}
} // namespace vstengine::dsp
