#include "PsyBassVoice.h"
#include <cmath>

namespace vstengine::dsp {

bool PsyBassVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PsyBassSound*>(sound) != nullptr;
}

void PsyBassVoice::setPitchEnvelope(const float semitones,
                                    const float seconds) noexcept
{
    pitchEnvSemitones = juce::jlimit(0.0f, 36.0f, semitones);
    pitchEnvSeconds = juce::jlimit(0.001f, 0.120f, seconds);
}

void PsyBassVoice::setAmpEnvelope(const float attack,
                                  const float decay,
                                  const float sustain,
                                  const float release) noexcept
{
    attackSeconds = juce::jlimit(0.0005f, 0.100f, attack);
    decaySeconds = juce::jlimit(0.005f, 0.300f, decay);
    sustainLevel = juce::jlimit(0.0f, 1.0f, sustain);
    releaseSeconds = juce::jlimit(0.005f, 0.300f, release);
    updateAmpEnvelope();
}

void PsyBassVoice::setFilter(const float newCutoffHz,
                             const float newResonanceQ)
{
    cutoffHz = juce::jlimit(40.0f, 18000.0f, newCutoffHz);
    resonanceQ = juce::jlimit(0.25f, 8.0f, newResonanceQ);

    if (getSampleRate() > 0.0) {
        filter.setCoefficients(
            juce::IIRCoefficients::makeLowPass(
                getSampleRate(),
                cutoffHz,
                resonanceQ));
    }
}

void PsyBassVoice::updateAmpEnvelope()
{
    juce::ADSR::Parameters p;
    p.attack = attackSeconds;
    p.decay = decaySeconds;
    p.sustain = sustainLevel;
    p.release = releaseSeconds;
    ampEnvelope.setParameters(p);
}

void PsyBassVoice::startNote(const int midiNoteNumber,
                             const float velocity,
                             juce::SynthesiserSound*,
                             const int)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    phase = 0.0;
    level = velocity;
    pitchEnvelopePosition = 0;
    pitchEnvelopeSamples = juce::jmax(
        1,
        static_cast<int>(getSampleRate() * pitchEnvSeconds));

    ampEnvelope.setSampleRate(getSampleRate());
    updateAmpEnvelope();
    ampEnvelope.noteOn();

    filter.reset();
    setFilter(cutoffHz, resonanceQ);
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
                                   const int startSample,
                                   const int numSamples)
{
    if (!isVoiceActive())
        return;

    for (int sample = 0; sample < numSamples; ++sample) {
        const auto t = juce::jlimit(
            0.0,
            1.0,
            static_cast<double>(pitchEnvelopePosition)
                / static_cast<double>(pitchEnvelopeSamples));

        const auto semitones =
            static_cast<double>(pitchEnvSemitones)
            * std::pow(1.0 - t, 2.0);

        const auto frequency =
            baseFrequency * std::pow(2.0, semitones / 12.0);

        const auto phaseDelta =
            juce::MathConstants<double>::twoPi
            * frequency / getSampleRate();

        const auto fundamental =
            static_cast<float>(std::sin(phase));
        const auto harmonic =
            static_cast<float>(0.20 * std::sin(phase * 2.0));

        const auto amp = ampEnvelope.getNextSample();
        const auto raw = (fundamental + harmonic) * level * amp;
        const auto shaped = std::tanh(raw * drive);
        const auto filtered = filter.processSingleSampleRaw(shaped) * 0.55f;

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.addSample(channel, startSample + sample, filtered);

        phase += phaseDelta;

        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        ++pitchEnvelopePosition;

        if (!ampEnvelope.isActive()) {
            clearCurrentNote();
            break;
        }
    }
}

} // namespace vstengine::dsp
