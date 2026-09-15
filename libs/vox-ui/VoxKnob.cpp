VoxKnob::VoxKnob (juce::String labelText, Size s) : size (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 0.75f,    // 135°
                                juce::MathConstants<float>::pi * 2.25f,    // 405°
                                true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    // shift+drag fine и mouse wheel единообразно задаются на уровне Slider
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    label.setText (std::move (labelText), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, tokens::colour::textSecondary);
    label.setFont (typography::controlLabel());
    addAndMakeVisible (label);
}

int VoxKnob::knobDiameter() const noexcept
{
    switch (size) {
        case Size::Small:  return tokens::size::knobSmall;
        case Size::Large:  return tokens::size::knobLarge;
        case Size::Normal:
        default:           return tokens::size::knobNormal;
    }
}

void VoxKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (18));
    const auto d = knobDiameter();
    slider.setBounds (area.withSizeKeepingCentre (d, d));
}
