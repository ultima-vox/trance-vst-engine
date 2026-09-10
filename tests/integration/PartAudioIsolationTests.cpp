#include "PluginProcessor.h"
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

double renderHostNote(int channel, bool bassMute = false,
                      bool kickMute = false, int bassChannel = 1,
                      int kickChannel = 2)
{
    VstEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setPlain(processor, "midiMode", 1.0f); // Piano Roll: generator disabled.
    setPlain(processor, "midiChannel", static_cast<float>(bassChannel));
    setPlain(processor, "kickMidiChannel", static_cast<float>(kickChannel));
    setPlain(processor, "bassMute", bassMute ? 1.0f : 0.0f);
    setPlain(processor, "kickMute", kickMute ? 1.0f : 0.0f);

    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(channel, 36, 1.0f), 0);
    processor.processBlock(audio, midi);

    double energy = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
            const double value = audio.getSample(ch, sample);
            energy += value * value;
        }
    return energy;
}

double renderAudition(bool bass, bool muteDestination)
{
    VstEngineAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setPlain(processor, "midiMode", 1.0f);
    setPlain(processor, bass ? "bassMute" : "kickMute",
             muteDestination ? 1.0f : 0.0f);
    setPlain(processor, bass ? "kickMute" : "bassMute", 0.0f);

    auto& keyboard = bass ? processor.bassKeyboardState()
                          : processor.kickKeyboardState();
    keyboard.noteOn(1, 36, 1.0f);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);

    double energy = 0.0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
            const double value = audio.getSample(ch, sample);
            energy += value * value;
        }
    return energy;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    constexpr double audible = 1.0e-7;
    constexpr double silent = 1.0e-12;

    require(renderHostNote(1, false, true) > audible,
            "CH1 renders Bass");
    require(renderHostNote(2, true, false) > audible,
            "CH2 renders Kick");
    require(renderHostNote(2, false, true) < silent,
            "CH2 never leaks to Bass");
    require(renderHostNote(1, true, false) < silent,
            "CH1 never leaks to Kick");
    require(renderHostNote(3) < silent,
            "unregistered CH3 renders neither Part");

    require(renderHostNote(5, false, true, 5, 9) > audible,
            "changed Bass CH5 routes Bass");
    require(renderHostNote(9, true, false, 5, 9) > audible,
            "changed Kick CH9 routes Kick");
    require(renderHostNote(1, false, false, 5, 9) < silent,
            "old Bass channel inactive after reroute");

    require(renderAudition(true, false) > audible,
            "Bass keyboard auditions Bass");
    require(renderAudition(true, true) < silent,
            "Bass keyboard does not audition Kick");
    require(renderAudition(false, false) > audible,
            "Kick keyboard auditions Kick");
    require(renderAudition(false, true) < silent,
            "Kick keyboard does not audition Bass");

    VstEngineAudioProcessor saved;
    setPlain(saved, "midiChannel", 5.0f);
    setPlain(saved, "kickMidiChannel", 9.0f);
    setPlain(saved, "bassMute", 1.0f);
    setPlain(saved, "kickSolo", 1.0f);
    setPlain(saved, "bassPan", -0.3f);
    setPlain(saved, "kickLevel", 0.4f);
    saved.partSequence(0)[3].gate = true;
    saved.partSequence(0)[3].noteOffset = 7;
    saved.partSequence(1)[6].gate = true;
    saved.partSequence(1)[6].velocity = 0.37f;
    juce::MemoryBlock projectState;
    saved.getStateInformation(projectState);

    VstEngineAudioProcessor restored;
    restored.setStateInformation(projectState.getData(),
                                 static_cast<int>(projectState.getSize()));
    const auto& restoredParts = restored.partRegistry();
    require(restoredParts[0].midiChannel == 5 && restoredParts[0].mute,
            "project state restores Bass routing/mix");
    require(restoredParts[1].midiChannel == 9 && restoredParts[1].solo,
            "project state restores Kick routing/mix");
    require(std::abs(restoredParts[0].pan + 0.3f) < 0.011f
                && std::abs(restoredParts[1].level - 0.4f) < 0.011f,
            "project state restores independent level/pan");
    require(restoredParts[0].sequence[3].gate
                && restoredParts[0].sequence[3].noteOffset == 7,
            "project state restores Bass sequence");
    require(restoredParts[1].sequence[6].gate
                && std::abs(restoredParts[1].sequence[6].velocity - 0.37f)
                       < 1.0e-6f,
            "project state restores Kick sequence");

    setPlain(restored, "midiChannel", 7.0f);
    restored.partSequence(0)[3].noteOffset = 11;
    juce::MemoryBlock resavedState;
    restored.getStateInformation(resavedState);
    VstEngineAudioProcessor reopened;
    reopened.setStateInformation(resavedState.getData(),
                                 static_cast<int>(resavedState.getSize()));
    require(reopened.partRegistry()[0].midiChannel == 7
                && reopened.partSequence(0)[3].noteOffset == 11,
            "load-edit-save-reopen uses latest Part state");

    std::cout << "Part audio isolation tests passed (" << testsPassed << "/"
              << testsRun << ")\n";
    return EXIT_SUCCESS;
}
