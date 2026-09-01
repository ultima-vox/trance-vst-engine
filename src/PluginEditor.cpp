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
    setSize(980, 690);

    titleLabel.setText(
        "VST ENGINE  /  DARK PSY CORE",
        juce::dontSendNotification);
    titleLabel.setFont(
        juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(
        juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    bassSectionLabel.setText("PSY BASS ENGINE", juce::dontSendNotification);
    bassSectionLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    bassSectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bassSectionLabel);

    for (auto* slider : {
             &driveSlider, &pitchEnvAmountSlider, &pitchEnvTimeSlider,
             &attackSlider, &decaySlider, &sustainSlider, &releaseSlider,
             &cutoffSlider, &resonanceSlider }) {
        configureRotary(*slider);
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

    configureLabel(driveLabel, "DRIVE");
    configureLabel(pitchEnvAmountLabel, "PITCH ENV");
    configureLabel(pitchEnvTimeLabel, "PITCH TIME");
    configureLabel(attackLabel, "ATTACK");
    configureLabel(decayLabel, "DECAY");
    configureLabel(sustainLabel, "SUSTAIN");
    configureLabel(releaseLabel, "RELEASE");
    configureLabel(cutoffLabel, "CUTOFF");
    configureLabel(resonanceLabel, "RESONANCE");
    configureLabel(midiModeLabel, "MIDI SOURCE");
    configureLabel(midiChannelLabel, "GEN MIDI CH");
    configureLabel(rootNoteLabel, "ROOT NOTE");

    driveAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "drive", driveSlider);
    pitchEnvAmountAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "pitchEnvAmount", pitchEnvAmountSlider);
    pitchEnvTimeAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "pitchEnvTime", pitchEnvTimeSlider);
    attackAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "attack", attackSlider);
    decayAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "decay", decaySlider);
    sustainAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "sustain", sustainSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "release", releaseSlider);
    cutoffAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "cutoff", cutoffSlider);
    resonanceAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "resonance", resonanceSlider);
    midiChannelAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "midiChannel", midiChannelSlider);
    rootNoteAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "rootNote", rootNoteSlider);
    midiModeAttachment = std::make_unique<ComboBoxAttachment>(
        processor.parameters(), "midiMode", midiModeBox);
}

void VstEngineAudioProcessorEditor::configureRotary(juce::Slider& slider)
{
    slider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow, false, 82, 22);
    addAndMakeVisible(slider);
}

void VstEngineAudioProcessorEditor::configureLabel(
    juce::Label& label,
    const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
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
    g.drawLine(
        40.0f, 190.0f,
        static_cast<float>(getWidth() - 40),
        190.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(13.0f);
    g.drawText(
        "Mouse keyboard, Cubase MIDI and generator share the same synth engine.",
        40, 648, getWidth() - 80, 22,
        juce::Justification::centredLeft);
}

void VstEngineAudioProcessorEditor::resized()
{
    titleLabel.setBounds(34, 32, getWidth() - 68, 32);

    midiModeLabel.setBounds(50, 96, 210, 22);
    midiModeBox.setBounds(50, 122, 210, 32);

    midiChannelLabel.setBounds(305, 96, 150, 22);
    midiChannelSlider.setBounds(318, 122, 124, 32);

    rootNoteLabel.setBounds(500, 96, 150, 22);
    rootNoteSlider.setBounds(513, 122, 124, 32);

    midiDragButton.setBounds(705, 118, 220, 36);

    bassSectionLabel.setBounds(45, 160, 240, 24);

    const int topY = 220;
    const int bottomY = 360;
    const int knobW = 110;
    const int knobH = 112;
    const int labelH = 22;

    struct KnobLayout {
        juce::Slider* slider;
        juce::Label* label;
        int x;
        int y;
    };

    const KnobLayout knobs[] = {
        { &driveSlider, &driveLabel, 45, topY },
        { &pitchEnvAmountSlider, &pitchEnvAmountLabel, 175, topY },
        { &pitchEnvTimeSlider, &pitchEnvTimeLabel, 305, topY },
        { &cutoffSlider, &cutoffLabel, 435, topY },
        { &resonanceSlider, &resonanceLabel, 565, topY },
        { &attackSlider, &attackLabel, 110, bottomY },
        { &decaySlider, &decayLabel, 250, bottomY },
        { &sustainSlider, &sustainLabel, 390, bottomY },
        { &releaseSlider, &releaseLabel, 530, bottomY }
    };

    for (const auto& item : knobs) {
        item.label->setBounds(item.x, item.y, knobW, labelH);
        item.slider->setBounds(item.x, item.y + labelH, knobW, knobH);
    }

    pianoKeyboard.setBounds(40, 525, getWidth() - 80, 105);
}

juce::AudioProcessorEditor*
VstEngineAudioProcessor::createEditor()
{
    return new VstEngineAudioProcessorEditor(*this);
}
