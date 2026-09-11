#include "PluginProcessor.h"
#include "instrument/HostParameterSchema.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int testsRun = 0;
void require(bool condition, const char* message)
{
    ++testsRun;
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void setPlain(VstEngineAudioProcessor& processor, const std::string& id,
              float value)
{
    auto* parameter = processor.parameters().getParameter(id);
    require(parameter != nullptr, id.c_str());
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
double energy(const juce::AudioBuffer<float>& audio)
{
    double result = 0.0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
            const auto value = audio.getSample(channel, sample);
            require(std::isfinite(value), "render stays finite");
            result += static_cast<double>(value) * value;
        }
    return result;
}
double renderHostNote(VstEngineAudioProcessor& processor, int channel,
                      int note = 36)
{
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(channel, note, 1.0f), 0);
    processor.processBlock(audio, midi);
    return energy(audio);
}
double renderAudition(VstEngineAudioProcessor& processor, std::size_t slot)
{
    processor.selectSlot(slot);
    processor.keyboardState().noteOn(1, 48, 1.0f);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
    processor.keyboardState().noteOff(1, 48, 1.0f);
    return energy(audio);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double audible = 1.0e-7;
    constexpr double silent = 1.0e-12;

    auto processor = std::make_unique<VstEngineAudioProcessor>();
    processor->prepareToPlay(48000.0, 512);
    setPlain(*processor, "midiMode", 1.0f);
    for (std::size_t slot = 0; slot < vstengine::instrument::maxSlots; ++slot)
        for (std::size_t macro = 0;
             macro < vstengine::instrument::macrosPerSlot; ++macro) {
            const auto id = vstengine::instrument::hostparams::macroId(slot, macro);
            const auto* parameter = processor->parameters().getParameter(id);
            require(parameter != nullptr && parameter->isAutomatable(),
                    "stable slot macro exposed to host");
        }

    require(renderHostNote(*processor, 1) > audible, "CH1 renders Bass");
    processor->instrumentRack().reset();
    require(renderHostNote(*processor, 2, 48) > audible,
            "CH2 renders independent ReferenceInstrument");
    processor->instrumentRack().reset();
    require(renderHostNote(*processor, 3) < silent, "unassigned channel silent");

    setPlain(*processor, "slot01MidiIn", 5.0f);
    processor->instrumentRack().reset();
    require(renderHostNote(*processor, 1) < silent, "old Bass channel inactive");
    processor->instrumentRack().reset();
    require(renderHostNote(*processor, 5) > audible, "Bass channel configurable");

    processor->instrumentRack().reset();
    require(renderAudition(*processor, 0) > audible,
            "selected-slot keyboard auditions Bass");
    setPlain(*processor, "slot01Mute", 1.0f);
    processor->instrumentRack().reset();
    require(renderAudition(*processor, 0) < silent, "slot mute blocks audition");
    setPlain(*processor, "slot01Mute", 0.0f);

    processor->sequence()[3].gate = true;
    processor->sequence()[3].noteOffset = 7;
    processor->publishSequenceForAudio();
    setPlain(*processor, "kickTune", 43.0f);
    setPlain(*processor, "matchBassTimingOffsetMs", 12.0f);
    juce::MemoryBlock state;
    processor->getStateInformation(state);
    require(state.getSize() > 0, "state serialization succeeds");

    auto restored = std::make_unique<VstEngineAudioProcessor>();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    restored->prepareToPlay(48000.0, 512);
    setPlain(*restored, "midiMode", 1.0f);
    require(restored->sequence()[3].gate
                && restored->sequence()[3].noteOffset == 7,
            "Bass sequence round-trips");
    require(std::abs(restored->parameters().getRawParameterValue("kickTune")->load()
                     - 43.0f) < 0.01f,
            "dormant Kick value retained for migration");
    require(std::abs(restored->parameters().getRawParameterValue(
                         "matchBassTimingOffsetMs")->load() - 12.0f) < 0.01f,
            "dormant MATCH value retained for migration");
    require(renderHostNote(*restored, 9) < silent,
            "legacy Kick parameters never reactivate audio");

    std::cout << "Plugin Rack integration tests passed (" << testsRun << ")\n";
    return EXIT_SUCCESS;
}
