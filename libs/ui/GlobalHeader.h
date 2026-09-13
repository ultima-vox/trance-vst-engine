#pragma once
#include "common/UiComponents.h"

namespace vstengine::ui {
class GlobalHeader final : public juce::Component {
public:
    struct Callbacks {
        std::function<void()> previous, next, save, panic, settings;
        std::function<void(int)> choosePreset;
    };
    GlobalHeader (juce::AudioProcessorValueTreeState&, Callbacks);
    void paint (juce::Graphics&) override; void resized() override;
    void setPresetName (const juce::String&); void setTransportActive (bool);
    void setPresetEntries(const juce::StringArray&, int selectedIndex);
    void setMidiActivity(bool); void setCpuLoad(float);
private:
    void randomizeSeed();
    juce::AudioProcessorValueTreeState& state;
    juce::Label brand, midi, cpu, outputLabel;
    juce::ComboBox preset;
    juce::StringArray presetNames;
    juce::TextButton previousButton { "<" }, nextButton { ">" },
        saveButton { "Save" }, seedButton { "Seed" },
        panicButton { "Panic" }, settingsButton { "Settings" };
    juce::Slider output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
};
} // namespace vstengine::ui
