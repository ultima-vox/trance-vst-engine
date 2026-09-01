#include "PsyBassVoice.h"
#include <cmath>

namespace vstengine::dsp {
bool PsyBassVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PsyBassSound*>(sound) != nullptr;
}

void PsyBassVoice::startNote(const int midiNoteNumber, const float velocity,
                             juce::SynthesiserSound*, const int)
{
    baseFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    phase = 0.0;
    level = velocity;
    pitchEnvelopePosition = 0;
    pitchEnvelopeSamples = juce::jmax(1, static_cast<int>(getSampleRate() * 0.018));

    juce::ADSR::Parameters p;
    p.attack = 0.001f;
    p.decay = 0.055f;
    p.sustain = 0.72f;
    p.release = releaseSeconds;
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

    for (int sample = 0; sample < numSamples; ++sample) {
        const auto t = juce::jlimit(
            0.0, 1.0,
            static_cast<double>(pitchEnvelopePosition) /
                static_cast<double>(pitchEnvelopeSamples));

        const auto semitones = 12.0 * std::pow(1.0 - t, 2.0);
        const auto frequency = baseFrequency * std::pow(2.0, semitones / 12.0);
        const auto phaseDelta =
            juce::MathConstants<double>::twoPi * frequency / getSampleRate();

        const auto fundamental = static_cast<float>(std::sin(phase));
        const auto harmonic = static_cast<float>(0.20 * std::sin(phase * 2.0));
        const auto raw = (fundamental + harmonic) * level * ampEnvelope.getNextSample();
        const auto shaped = std::tanh(raw * drive) * 0.55f;

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.addSample(channel, startSample + sample, shaped);

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
