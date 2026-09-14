#include "VoxLookAndFeel.h"
#include "Tokens.h"

#include <cmath>

namespace vox::ui {

VoxLookAndFeel::VoxLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, tokens::colour::background);
    setColour (juce::Label::textColourId, tokens::colour::text);
    setColour (juce::ComboBox::textColourId, tokens::colour::text);
    setColour (juce::ComboBox::backgroundColourId, tokens::colour::control);
    setColour (juce::ComboBox::outlineColourId, tokens::colour::border);
    setColour (juce::TextButton::textColourOffId, tokens::colour::textSecondary);
    setColour (juce::TextButton::textColourOnId, tokens::colour::background);
}

void VoxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                           const juce::Colour&, bool isMouseOverButton,
                                           bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = button.getToggleState() ? tokens::colour::accent : tokens::colour::control;
    auto border = button.getToggleState() ? tokens::colour::accent
                                          : (isMouseOverButton ? tokens::colour::accentDim
                                                               : tokens::colour::border);

    if (isButtonDown)
        fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, tokens::radius::small);
    g.setColour (border);
    g.drawRoundedRectangle (bounds, tokens::radius::small, 1.0f);
}

void VoxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider& slider)
{
    const auto diameter = static_cast<float> (juce::jmin (width, height)) - 6.0f;
    const auto radius = diameter * 0.5f;
    const auto centre = juce::Point<float> (static_cast<float> (x) + width * 0.5f,
                                            static_cast<float> (y) + height * 0.5f);
    const auto arcRadius = radius - 3.0f;
    const auto angle = rotaryStartAngle
                     + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path inactive;
    inactive.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (tokens::colour::border);
    g.strokePath (inactive, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (slider.isEnabled() ? tokens::colour::accent : tokens::colour::textMuted);
    g.strokePath (active, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    g.setColour (tokens::colour::panelRaised);
    g.fillEllipse (centre.x - radius + 7.0f, centre.y - radius + 7.0f,
                   diameter - 14.0f, diameter - 14.0f);

    const auto pointerLength = radius - 11.0f;
    const auto pointerStart = centre + juce::Point<float> (0.0f, -4.0f).rotatedAboutOrigin (angle);
    const auto pointerEnd = centre + juce::Point<float> (0.0f, -pointerLength).rotatedAboutOrigin (angle);
    g.setColour (tokens::colour::text);
    g.drawLine ({ pointerStart, pointerEnd }, 2.0f);
}

void VoxLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int buttonX, int buttonY, int buttonW, int buttonH,
                                   juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                                 static_cast<float> (width) - 1.0f,
                                                 static_cast<float> (height) - 1.0f);
    g.setColour (tokens::colour::control);
    g.fillRoundedRectangle (bounds, tokens::radius::small);
    g.setColour (box.hasKeyboardFocus (true) ? tokens::colour::accent : tokens::colour::border);
    g.drawRoundedRectangle (bounds, tokens::radius::small, 1.0f);

    const auto arrowArea = juce::Rectangle<float> (static_cast<float> (buttonX),
                                                    static_cast<float> (buttonY),
                                                    static_cast<float> (buttonW),
                                                    static_cast<float> (buttonH)).reduced (8.0f);
    juce::Path arrow;
    arrow.startNewSubPath (arrowArea.getX(), arrowArea.getCentreY() - 2.0f);
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    arrow.lineTo (arrowArea.getRight(), arrowArea.getCentreY() - 2.0f);
    g.setColour (tokens::colour::textSecondary);
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

} // namespace vox::ui
