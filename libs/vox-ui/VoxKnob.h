#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Tokens.h"

namespace vox::ui {

class VoxKnob : public juce::Component,
                public juce::SettableTooltipClient
{
public:
    enum class Size { Small, Normal, Large };
    enum class Style { Standard, Bipolar, Modulated };

    explicit VoxKnob (juce::String labelText = {}, Size s = Size::Normal);

    juce::Slider& getSlider() noexcept { return slider; }
    const juce::Slider& getSlider() const noexcept { return slider; }

    void setLabel (juce::String text);
    void setSize (Size s);                 // переключает визуальный размер
    void setModulationAmount (float amount01);   // 0..1 -> длина mod-ring
    void setStyle (Style s);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::Label  label;
    Size  size  = Size::Normal;
    Style style = Style::Standard;
    float modulationAmount = 0.0f;

    int knobDiameter() const noexcept;
};

} // namespace vox::ui
