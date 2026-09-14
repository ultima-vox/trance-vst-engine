#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vox::ui {

class VoxPanel : public juce::Component {
public:
    explicit VoxPanel (juce::String title = {});
    void setTitle (juce::String newTitle);
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> getContentBounds() const;

private:
    juce::String title;
};

class VoxKnob : public juce::Component {
public:
    explicit VoxKnob (juce::String labelText = {});

    juce::Slider& getSlider() noexcept { return slider; }
    const juce::Slider& getSlider() const noexcept { return slider; }
    void setLabel (juce::String text);
    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
};

class VoxSectionHeader : public juce::Component {
public:
    explicit VoxSectionHeader (juce::String text = {});
    void setText (juce::String text);
    void paint (juce::Graphics&) override;

private:
    juce::String text;
};

} // namespace vox::ui
