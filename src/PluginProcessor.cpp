#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

VstEngineAudioProcessor::VstEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      pattern(vstengine::generator::PatternGenerator::generate(
          vstengine::generator::Style::darkPsy, 0xD4A4u))
{
    for (int i = 0; i < 4; ++i)
        synth.addVoice(new vstengine::dsp::PsyBassVoice());
    synth.addSound(new vstengine::dsp::PsyBassSound());
}

void VstEngineAudioProcessor::prepareToPlay(const double sampleRate, const int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    currentStep = 0;
    heldNote = -1;
    samplesUntilNextStep = 0.0;
    samplesUntilNoteOff = -1.0;
}

bool VstEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void VstEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    addGeneratedMidi(midi, buffer.getNumSamples());

    const auto drive = apvts.getRawParameterValue("drive")->load();
    const auto release = apvts.getRawParameterValue("release")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice =
                dynamic_cast<vstengine::dsp::PsyBassVoice*>(synth.getVoice(i))) {
            voice->setDrive(drive);
            voice->setRelease(release);
        }
    }

    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
}

void VstEngineAudioProcessor::addGeneratedMidi(juce::MidiBuffer& midi,
                                                const int numSamples)
{
    double bpm = 145.0;
    bool playing = true;

    if (auto* playHead = getPlayHead()) {
        if (const auto position = playHead->getPosition()) {
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            playing = position->getIsPlaying();
        }
    }

    if (!playing) {
        if (heldNote >= 0)
            midi.addEvent(juce::MidiMessage::noteOff(1, heldNote), 0);
        heldNote = -1;
        samplesUntilNoteOff = -1.0;
        samplesUntilNextStep = 0.0;
        return;
    }

    const auto samplesPerQuarter =
        currentSampleRate * 60.0 / juce::jmax(20.0, bpm);
    const auto samplesPerStep = samplesPerQuarter / 4.0;

    for (int offset = 0; offset < numSamples; ++offset) {
        if (samplesUntilNoteOff >= 0.0) {
            samplesUntilNoteOff -= 1.0;
            if (samplesUntilNoteOff <= 0.0 && heldNote >= 0) {
                midi.addEvent(juce::MidiMessage::noteOff(1, heldNote), offset);
                heldNote = -1;
                samplesUntilNoteOff = -1.0;
            }
        }

        samplesUntilNextStep -= 1.0;
        if (samplesUntilNextStep <= 0.0) {
            const auto& step = pattern[static_cast<std::size_t>(currentStep)];

            if (step.gate) {
                constexpr int rootNote = 36;
                heldNote = rootNote + step.noteOffset;
                const auto velocity = step.accent ? 0.95f : 0.72f;
                midi.addEvent(
                    juce::MidiMessage::noteOn(1, heldNote, velocity), offset);
                samplesUntilNoteOff = samplesPerStep * 0.62;
            }

            currentStep = (currentStep + 1)
                % static_cast<int>(pattern.size());
            samplesUntilNextStep += samplesPerStep;
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
VstEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> { 1.0f, 6.0f, 0.01f }, 1.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "release", 1 }, "Release",
        juce::NormalisableRange<float> { 0.005f, 0.250f, 0.001f, 0.5f },
        0.035f, "s"));

    return { params.begin(), params.end() };
}

void VstEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void VstEngineAudioProcessor::setStateInformation(const void* data,
                                                   const int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VstEngineAudioProcessor();
}
