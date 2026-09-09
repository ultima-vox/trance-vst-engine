#pragma once
#include "common/UiComponents.h"

namespace vstengine::ui {
class GlobalHeader final : public juce::Component {
public:
    struct Callbacks { std::function<void()> previous, next, save, panic; };
    GlobalHeader (juce::AudioProcessorValueTreeState&, Callbacks);
    void paint (juce::Graphics&) override; void resized() override;
    void setPresetName (const juce::String&); void setTransportActive (bool);
private:
    void randomizeSeed();
    juce::AudioProcessorValueTreeState& state;
    juce::Label brand, preset, sync;
    juce::TextButton previousButton { "< Prev" }, nextButton { "Next >" }, saveButton { "Save" }, seedButton { "Seed" }, panicButton { "Panic" };
    juce::Slider output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
};
} // namespace vstengine::ui
