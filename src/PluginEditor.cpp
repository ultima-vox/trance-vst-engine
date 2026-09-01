#include "PluginEditor.h"

VstEngineAudioProcessorEditor::VstEngineAudioProcessorEditor(
    VstEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(620, 360);

    titleLabel.setText(
        "VST ENGINE  /  DARK PSY CORE",
        juce::dontSendNotification);
    titleLabel.setFont(
        juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(
        juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    for (auto* slider : { &driveSlider, &releaseSlider }) {
        slider->setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(
            juce::Slider::TextBoxBelow, false, 88, 24);
        addAndMakeVisible(slider);
    }

    driveLabel.setText("DRIVE", juce::dontSendNotification);
    releaseLabel.setText("RELEASE", juce::dontSendNotification);

    for (auto* label : { &driveLabel, &releaseLabel }) {
        label->setJustificationType(
            juce::Justification::centred);
        addAndMakeVisible(label);
    }

    driveAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "drive", driveSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "release", releaseSlider);
}

void VstEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(11, 15, 26));

    const auto bounds =
        getLocalBounds().toFloat().reduced(22.0f);
    g.setColour(juce::Colour::fromRGB(45, 53, 70));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    g.setColour(juce::Colour::fromRGB(185, 164, 105));
    g.drawLine(
        24.0f, 76.0f,
        static_cast<float>(getWidth() - 24),
        76.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(14.0f);
    g.drawText(
        "Host-synced 16-step rolling bass generator / prototype 0.1",
        34, 294, getWidth() - 68, 30,
        juce::Justification::centredLeft);
}

void VstEngineAudioProcessorEditor::resized()
{
    titleLabel.setBounds(34, 32, getWidth() - 68, 32);

    driveLabel.setBounds(100, 102, 150, 24);
    driveSlider.setBounds(100, 126, 150, 150);

    releaseLabel.setBounds(370, 102, 150, 24);
    releaseSlider.setBounds(370, 126, 150, 150);
}

juce::AudioProcessorEditor*
VstEngineAudioProcessor::createEditor()
{
    return new VstEngineAudioProcessorEditor(*this);
}
