#include "UiComponents.h"

namespace vstengine::ui {

VoxLookAndFeel::VoxLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, colours::panelRaised);
    setColour(juce::ComboBox::outlineColourId, colours::border);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              colours::primary.withAlpha(0.22f));
    setColour(juce::Slider::rotarySliderFillColourId, colours::primary);
    setColour(juce::Slider::rotarySliderOutlineColourId, colours::inactive);
}

void VoxLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour& base,
    bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = button.getToggleState() ? colours::primary.darker(0.55f)
        : base;
    if (highlighted) colour = colour.brighter(0.08f);
    if (down) colour = colour.darker(0.12f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds, metrics::corner);
    g.setColour(button.getToggleState() ? colours::primary : colours::border);
    g.drawRoundedRectangle(bounds, metrics::corner, 1.0f);
}

void VoxLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float position,
    float start, float end, juce::Slider&)
{
    const auto radius = 0.5f * static_cast<float>(juce::jmin(width, height))
        - 5.0f;
    const auto centre = juce::Point<float>(x + width * 0.5f,
                                           y + height * 0.5f);
    const auto bounds = juce::Rectangle<float>(centre.x - radius,
        centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(colours::shadow);
    g.fillEllipse(bounds.translated(0.0f, 2.0f));
    g.setColour(colours::inactive);
    g.fillEllipse(bounds);
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                      0.0f, start, start + position * (end - start), true);
    g.setColour(colours::primary);
    g.strokePath(arc, juce::PathStrokeType(2.5f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto angle = start + position * (end - start);
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.2f, -radius + 7.0f, 2.4f,
                                radius * 0.42f, 1.2f);
    g.setColour(colours::text);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle)
        .translated(centre.x, centre.y));
}

void VoxLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                  bool, int, int, int, int,
                                  juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width),
                                          static_cast<float>(height));
    g.setColour(colours::panelRaised);
    g.fillRoundedRectangle(bounds.reduced(0.5f), metrics::corner);
    g.setColour(box.hasKeyboardFocus(true) ? colours::primary
                                           : colours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), metrics::corner, 1.0f);
    juce::Path arrow;
    const auto cx = static_cast<float>(width - 16);
    const auto cy = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f,
                      cx, cy + 3.0f);
    g.setColour(colours::mutedText);
    g.fillPath(arrow);
}

void styleButton (juce::Button& button, bool isPrimary)
{
    button.setColour (juce::TextButton::buttonColourId,
                      isPrimary ? colours::primary.darker (0.45f) : colours::panelRaised);
    button.setColour (juce::TextButton::buttonOnColourId, colours::primary.darker (0.25f));
    button.setColour (juce::TextButton::textColourOffId, colours::text);
    button.setColour (juce::TextButton::textColourOnId, colours::text);
}

void styleLabel (juce::Label& label, float size, juce::Justification justification,
                 juce::Colour colour)
{
    label.setFont (juce::FontOptions (size));
    label.setJustificationType (justification);
    label.setColour (juce::Label::textColourId, colour);
}

ParameterKnob::ParameterKnob (juce::AudioProcessorValueTreeState& state,
                              const char* parameterId, juce::String labelText,
                              juce::String tooltip)
{
    label.setText (std::move (labelText), juce::dontSendNotification);
    styleLabel (label, 12.0f, juce::Justification::centred, colours::mutedText);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colours::primary);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, colours::border);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background);
    slider.setColour (juce::Slider::textBoxOutlineColourId, colours::border);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterId, slider);
}

void ParameterKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (22));
    slider.setBounds (area.reduced (2));
}

ParameterSection::ParameterSection (juce::String titleText)
{
    title.setText (std::move (titleText), juce::dontSendNotification);
    styleLabel (title, 13.0f, juce::Justification::centredLeft, colours::primary);
    addAndMakeVisible (title);
}

ParameterKnob& ParameterSection::addKnob (juce::AudioProcessorValueTreeState& state,
                                          const char* parameterId, juce::String labelText,
                                          juce::String tooltip)
{
    auto knob = std::make_unique<ParameterKnob> (state, parameterId,
                                                  std::move (labelText), std::move (tooltip));
    auto& result = *knob;
    addAndMakeVisible (result);
    knobs.push_back (std::move (knob));
    return result;
}

void ParameterSection::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void ParameterSection::resized()
{
    auto area = getLocalBounds().reduced (12);
    title.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    row.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    row.alignItems = juce::FlexBox::AlignItems::stretch;
    for (auto& knob : knobs)
        row.items.add (juce::FlexItem (*knob).withFlex (1.0f).withMinWidth (82.0f));
    row.performLayout (area);
}

} // namespace vstengine::ui
