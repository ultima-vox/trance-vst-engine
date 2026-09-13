#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace vstengine::ui {

namespace metrics {
inline constexpr int spaceXs = 4;
inline constexpr int spaceSm = 8;
inline constexpr int spaceMd = 12;
inline constexpr int spaceLg = 18;
inline constexpr float corner = 7.0f;
}

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
inline const auto inactive = juce::Colour::fromRGB (32, 46, 63);
inline const auto shadow = juce::Colour::fromRGBA (0, 0, 0, 88);
}

class VoxLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    VoxLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                          float, float, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int,
                      juce::ComboBox&) override;
};

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
