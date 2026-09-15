#include "VoxComponents.h"
#include "Tokens.h"
#include "Typography.h"

namespace vox::ui {

VoxPanel::VoxPanel (juce::String titleText) : title (std::move (titleText)) {}

void VoxPanel::setTitle (juce::String newTitle)
{
    title = std::move (newTitle);
    repaint();
}

void VoxPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (tokens::colour::panel);
    g.fillRoundedRectangle (bounds, tokens::radius::medium);
    g.setColour (tokens::colour::border);
    g.drawRoundedRectangle (bounds, tokens::radius::medium, 1.0f);

    if (title.isNotEmpty()) {
        g.setColour (tokens::colour::textSecondary);
        g.setFont (typography::sectionTitle);
        g.drawText (title.toUpperCase(),
                    getLocalBounds().reduced (tokens::spacing::md).removeFromTop (20),
                    juce::Justification::centredLeft, false);
    }
}

juce::Rectangle<int> VoxPanel::getContentBounds() const
{
    auto area = getLocalBounds().reduced (tokens::spacing::md);
    if (title.isNotEmpty())
        area.removeFromTop (24);
    return area;
}

VoxKnob::VoxKnob (juce::String labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setVelocityBasedMode (false);
    addAndMakeVisible (slider);

    label.setText (std::move (labelText), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, tokens::colour::textSecondary);
    label.setFont (typography::controlLabel);
    addAndMakeVisible (label);
}

void VoxKnob::setLabel (juce::String text)
{
    label.setText (std::move (text), juce::dontSendNotification);
}

void VoxKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (20));
    slider.setBounds (area);
}

VoxSectionHeader::VoxSectionHeader (juce::String initialText) : text (std::move (initialText)) {}

void VoxSectionHeader::setText (juce::String newText)
{
    text = std::move (newText);
    repaint();
}

void VoxSectionHeader::paint (juce::Graphics& g)
{
    g.setColour (tokens::colour::textSecondary);
    g.setFont (typography::sectionTitle);
    g.drawText (text.toUpperCase(), getLocalBounds(), juce::Justification::centredLeft, false);
}

} // namespace vox::ui
