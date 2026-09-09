#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace vstengine::ui {

namespace colours {
inline const auto background = juce::Colour::fromRGB (8, 13, 22);
inline const auto panel = juce::Colour::fromRGB (19, 29, 43);
inline const auto panelRaised = juce::Colour::fromRGB (26, 39, 56);
inline const auto border = juce::Colour::fromRGB (48, 67, 88);
inline const auto primary = juce::Colour::fromRGB (55, 201, 235);
inline const auto status = juce::Colour::fromRGB (93, 190, 139);
inline const auto warning = juce::Colour::fromRGB (211, 113, 82);
inline const auto text = juce::Colour::fromRGB (225, 233, 241);
inline const auto mutedText = juce::Colour::fromRGB (137, 154, 174);
}

class ParameterKnob final : public juce::Component {
public:
    ParameterKnob (juce::AudioProcessorValueTreeState&, const char* parameterId,
                   juce::String labelText, juce::String tooltip = {});
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ParameterSection final : public juce::Component {
public:
    explicit ParameterSection (juce::String titleText);
    ParameterKnob& addKnob (juce::AudioProcessorValueTreeState&, const char* parameterId,
                            juce::String label, juce::String tooltip = {});
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title;
    std::vector<std::unique_ptr<ParameterKnob>> knobs;
};

void styleButton (juce::Button&, bool primary = false);
void styleLabel (juce::Label&, float size, juce::Justification,
                 juce::Colour colour = colours::text);

} // namespace vstengine::ui
