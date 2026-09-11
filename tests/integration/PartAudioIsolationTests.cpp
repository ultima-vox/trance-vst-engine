#include "PluginProcessor.h"
#include "instrument/HostParameterSchema.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int testsRun = 0;
int testsPassed = 0;
void require(bool condition, const char* message)
{
    ++testsRun;
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
    ++testsPassed;
}
void setPlain(VstEngineAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters().getParameter(id);
    require(parameter != nullptr, id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
double energy(const juce::AudioBuffer<float>& audio)
{
    double result = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            result += std::pow(audio.getSample(ch, sample), 2.0);
    return result;
}
double renderHostNote(int channel, int bassChannel = 1)
{
    VstEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setPlain(processor, "midiMode", 1.0f);
    setPlain(processor, "midiChannel", static_cast<float>(bassChannel));
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(channel, 36, 1.0f), 0);
    processor.processBlock(audio, midi);
    return energy(audio);
}
double renderBassAudition(bool muted)
{
    VstEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setPlain(processor, "midiMode", 1.0f);
    setPlain(processor, "bassMute", muted ? 1.0f : 0.0f);
    processor.bassKeyboardState().noteOn(1, 36, 1.0f);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
    return energy(audio);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double audible = 1.0e-7;
    constexpr double silent = 1.0e-12;
    {
        VstEngineAudioProcessor automation;
        for (std::size_t slot = 0; slot < vstengine::instrument::maxSlots; ++slot)
            for (std::size_t macro = 0;
                 macro < vstengine::instrument::macrosPerSlot; ++macro) {
                const auto id = vstengine::instrument::hostparams::macroId(slot, macro);
                const auto* parameter = automation.parameters().getParameter(id);
                require(parameter != nullptr && parameter->isAutomatable(),
                        "stable slot macro exposed to host");
            }
    }
    require(renderHostNote(1) > audible, "CH1 renders Bass");
    require(renderHostNote(2) < silent, "removed Kick CH2 stays silent");
    require(renderHostNote(16) < silent, "unassigned channels stay silent");
    require(renderHostNote(5, 5) > audible, "Bass channel remains configurable");
    require(renderHostNote(1, 5) < silent, "old Bass channel inactive after reroute");
    require(renderBassAudition(false) > audible, "Bass keyboard auditions Bass");
    require(renderBassAudition(true) < silent, "Bass mute blocks audition");

    VstEngineAudioProcessor saved;
    setPlain(saved, "midiChannel", 5.0f);
    setPlain(saved, "kickMidiChannel", 9.0f);
    setPlain(saved, "kickTune", 43.0f);
    setPlain(saved, "matchBassTimingOffsetMs", 12.0f);
    saved.partSequence(0)[3].gate = true;
    saved.partSequence(0)[3].noteOffset = 7;
    saved.partRegistry()[1].enabled = true;
    saved.partSequence(1)[6].gate = true;
    juce::MemoryBlock state;
    saved.getStateInformation(state);

    VstEngineAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(restored.partRegistry()[0].midiChannel == 5,
            "legacy project restores Bass routing");
    require(restored.partSequence(0)[3].gate
                && restored.partSequence(0)[3].noteOffset == 7,
            "legacy project restores Bass sequence");
    require(restored.partSequence(1)[6].gate,
            "removed Kick sequence retained for migration");
    require(!restored.partRegistry()[1].enabled,
            "removed Kick cannot reactivate from legacy state");
    require(std::abs(restored.parameters().getRawParameterValue("kickTune")->load()
                     - 43.0f) < 0.01f,
            "legacy Kick APVTS value retained");
    require(std::abs(restored.parameters().getRawParameterValue(
                         "matchBassTimingOffsetMs")->load() - 12.0f) < 0.01f,
            "legacy MATCH APVTS value retained");

    restored.prepareToPlay(48000.0, 512);
    setPlain(restored, "midiMode", 1.0f);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(9, 36, 1.0f), 0);
    restored.processBlock(audio, midi);
    require(energy(audio) < silent, "legacy Kick route never renders audio");

    std::cout << "7A product split tests passed (" << testsPassed << "/"
              << testsRun << ")\n";
    return EXIT_SUCCESS;
}
