#include "PluginEditor.h"

VstEngineAudioProcessorEditor::VstEngineAudioProcessorEditor(
    VstEngineAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      midiDragButton(p),
      pianoKeyboard(
          p.keyboardState(),
          juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize(760, 590);

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

    for (auto* slider : { &midiChannelSlider, &rootNoteSlider }) {
        slider->setSliderStyle(juce::Slider::IncDecButtons);
        slider->setTextBoxStyle(
            juce::Slider::TextBoxLeft, false, 72, 24);
        addAndMakeVisible(slider);
    }

    midiModeBox.addItem("AUTO", 1);
    midiModeBox.addItem("PIANO ROLL", 2);
    midiModeBox.addItem("GENERATOR", 3);
    midiModeBox.addItem("BOTH", 4);
    addAndMakeVisible(midiModeBox);

    midiDragButton.setTooltip(
        "Drag the current generated pattern into Cubase as a MIDI clip.");
    addAndMakeVisible(midiDragButton);

    pianoKeyboard.setAvailableRange(24, 84);
    pianoKeyboard.setLowestVisibleKey(24);
    pianoKeyboard.setKeyWidth(18.0f);
    pianoKeyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(pianoKeyboard);

    driveLabel.setText("DRIVE", juce::dontSendNotification);
    releaseLabel.setText("RELEASE", juce::dontSendNotification);
    midiModeLabel.setText("MIDI SOURCE", juce::dontSendNotification);
    midiChannelLabel.setText("GEN MIDI CH", juce::dontSendNotification);
    rootNoteLabel.setText("ROOT NOTE", juce::dontSendNotification);

    for (auto* label : {
             &driveLabel, &releaseLabel, &midiModeLabel,
             &midiChannelLabel, &rootNoteLabel }) {
        label->setJustificationType(
            juce::Justification::centred);
        addAndMakeVisible(label);
    }

    driveAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "drive", driveSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "release", releaseSlider);
    midiChannelAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "midiChannel", midiChannelSlider);
    rootNoteAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "rootNote", rootNoteSlider);
    midiModeAttachment = std::make_unique<ComboBoxAttachment>(
        processor.parameters(), "midiMode", midiModeBox);
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
        "AUTO: piano roll wins when MIDI notes are present; otherwise generator",
        40, 505, getWidth() - 80, 24,
        juce::Justification::centredLeft);

    g.drawText(
        "Generator MIDI is exposed to the host for recording/routing.",
        40, 528, getWidth() - 80, 24,
        juce::Justification::centredLeft);

    g.drawText(
        "Drag exports the current 16-step pattern using the selected root/channel.",
        40, 551, getWidth() - 80, 24,
        juce::Justification::centredLeft);
}

void VstEngineAudioProcessorEditor::resized()
{
    titleLabel.setBounds(34, 32, getWidth() - 68, 32);

    midiModeLabel.setBounds(55, 100, 210, 24);
    midiModeBox.setBounds(55, 128, 210, 32);

    midiChannelLabel.setBounds(290, 100, 155, 24);
    midiChannelSlider.setBounds(305, 128, 125, 32);

    rootNoteLabel.setBounds(480, 100, 155, 24);
    rootNoteSlider.setBounds(495, 128, 125, 32);

    driveLabel.setBounds(150, 195, 150, 24);
    driveSlider.setBounds(150, 219, 150, 150);

    releaseLabel.setBounds(455, 195, 150, 24);
    releaseSlider.setBounds(455, 219, 150, 150);

    midiDragButton.setBounds(250, 365, 260, 34);
    pianoKeyboard.setBounds(40, 415, getWidth() - 80, 78);
}

juce::AudioProcessorEditor*
VstEngineAudioProcessor::createEditor()
{
    return new VstEngineAudioProcessorEditor(*this);
}
