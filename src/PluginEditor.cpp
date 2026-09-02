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
    setSize(1100, 750);

    // ── Title ──────────────────────────────────────────────
    titleLabel.setText(
        "VST ENGINE  /  DARK PSY CORE",
        juce::dontSendNotification);
    titleLabel.setFont(
        juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(
        juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // ── Rotary sliders helper ──────────────────────────────
    auto setupRotary = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 24);
        addAndMakeVisible(&s);
    };

    // ── Generator settings (inc/dec) ──────────────────────
    for (auto* s : { &midiChannelSlider, &rootNoteSlider }) {
        s->setSliderStyle(juce::Slider::IncDecButtons);
        s->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 72, 24);
        addAndMakeVisible(s);
    }

    // ── MIDI Mode combobox ─────────────────────────────────
    midiModeBox.addItem("AUTO", 1);
    midiModeBox.addItem("PIANO ROLL", 2);
    midiModeBox.addItem("GENERATOR", 3);
    midiModeBox.addItem("BOTH", 4);
    addAndMakeVisible(midiModeBox);

    // ── Pitch envelope rotary sliders ──────────────────────
    for (auto* s : { &pitchEnvAmountSlider, &pitchEnvTimeSlider,
                     &pitchEnvCurveSlider }) {
        setupRotary(*s);
    }

    // ── ADSR rotary sliders ────────────────────────────────
    for (auto* s : { &ampAttackSlider, &ampDecaySlider,
                     &ampSustainSlider, &ampReleaseSlider }) {
        setupRotary(*s);
    }

    // ── Filter rotary sliders ──────────────────────────────
    for (auto* s : { &filterCutoffSlider, &filterResonanceSlider,
                     &filterDriveSlider, &keyTrackingSlider }) {
        setupRotary(*s);
    }

    // ── Output rotary sliders ──────────────────────────────
    for (auto* s : { &driveSlider, &outputLevelSlider }) {
        setupRotary(*s);
    }

    // ── MIDI drag button ───────────────────────────────────
    midiDragButton.setTooltip(
        "Drag the current generated pattern into Cubase as a MIDI clip.");
    addAndMakeVisible(midiDragButton);

    // ── Piano keyboard ─────────────────────────────────────
    pianoKeyboard.setAvailableRange(24, 84);
    pianoKeyboard.setLowestVisibleKey(24);
    pianoKeyboard.setKeyWidth(18.0f);
    pianoKeyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(pianoKeyboard);

    // ── Labels ─────────────────────────────────────────────
    auto setupLabel = [this](juce::Label& l, juce::String text) {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(&l);
    };

    setupLabel(midiModeLabel, "MIDI SOURCE");
    setupLabel(midiChannelLabel, "GEN MIDI CH");
    setupLabel(rootNoteLabel, "ROOT NOTE");

    setupLabel(pitchEnvAmountLabel, "PITCH ENV AMT");
    setupLabel(pitchEnvTimeLabel, "PITCH ENV TIME");
    setupLabel(pitchEnvCurveLabel, "PITCH ENV CURVE");

    setupLabel(ampAttackLabel, "ATTACK");
    setupLabel(ampDecayLabel, "DECAY");
    setupLabel(ampSustainLabel, "SUSTAIN");
    setupLabel(ampReleaseLabel, "RELEASE");

    setupLabel(filterCutoffLabel, "CUTOFF");
    setupLabel(filterResonanceLabel, "RESONANCE");
    setupLabel(filterDriveLabel, "FILTER DRIVE");
    setupLabel(keyTrackingLabel, "KEY TRACK");

    setupLabel(driveLabel, "DRIVE");
    setupLabel(outputLevelLabel, "OUTPUT LEVEL");

    // ── Parameter attachments ──────────────────────────────
    midiModeAttachment = std::make_unique<ComboBoxAttachment>(
        processor.parameters(), "midiMode", midiModeBox);
    midiChannelAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "midiChannel", midiChannelSlider);
    rootNoteAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "rootNote", rootNoteSlider);

    pitchEnvAmountAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "pitchEnvAmount", pitchEnvAmountSlider);
    pitchEnvTimeAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "pitchEnvTime", pitchEnvTimeSlider);
    pitchEnvCurveAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "pitchEnvCurve", pitchEnvCurveSlider);

    ampAttackAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "ampAttack", ampAttackSlider);
    ampDecayAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "ampDecay", ampDecaySlider);
    ampSustainAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "ampSustain", ampSustainSlider);
    ampReleaseAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "ampRelease", ampReleaseSlider);

    filterCutoffAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "filterCutoff", filterCutoffSlider);
    filterResonanceAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "filterResonance", filterResonanceSlider);
    filterDriveAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "filterDrive", filterDriveSlider);
    keyTrackingAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "keyTracking", keyTrackingSlider);

    driveAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "drive", driveSlider);
    outputLevelAttachment = std::make_unique<SliderAttachment>(
        processor.parameters(), "outputLevel", outputLevelSlider);
}

void VstEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(11, 15, 26));

    const auto bounds =
        getLocalBounds().toFloat().reduced(22.0f);
    g.setColour(juce::Colour::fromRGB(45, 53, 70));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    // Section divider lines
    g.setColour(juce::Colour::fromRGB(185, 164, 105));
    g.setLineWidth(1.0f);

    // Horizontal divider under title row
    g.drawLine(24.0f, 76.0f,
               static_cast<float>(getWidth() - 24), 76.0f);

    // Section dividers
    const int sectionY[] = { 200, 340, 480, 560 };
    g.setColour(juce::Colour::fromRGB(45, 53, 70));
    for (int y : sectionY) {
        g.drawLine(24.0f, static_cast<float>(y),
                   static_cast<float>(getWidth() - 24), static_cast<float>(y));
    }

    // Section labels
    g.setColour(juce::Colour::fromRGB(185, 164, 105).withAlpha(0.4f));
    g.setFont(11.0f);
    g.drawText("PITCH ENVELOPE", 30, 182, 200, 16, juce::Justification::centredLeft);
    g.drawText("ADSR ENVELOPE", 30, 322, 200, 16, juce::Justification::centredLeft);
    g.drawText("FILTER", 30, 462, 200, 16, juce::Justification::centredLeft);
    g.drawText("OUTPUT", 30, 542, 200, 16, juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.setFont(11.0f);
    g.drawText(
        "Generator MIDI is exposed to the host for recording/routing.",
        40, 690, getWidth() - 80, 18,
        juce::Justification::centredLeft);

    g.drawText(
        "Drag exports the current 16-step pattern using the selected root/channel.",
        40, 710, getWidth() - 80, 18,
        juce::Justification::centredLeft);
}

void VstEngineAudioProcessorEditor::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();

    // ── Row 1: Title + MIDI Source ─────────────────────────
    titleLabel.setBounds(34, 32, w - 68, 32);

    midiModeLabel.setBounds(w - 320, 32, 120, 24);
    midiModeBox.setBounds(w - 320, 56, 200, 28);

    // ── Row 2: Generator settings ──────────────────────────
    midiChannelLabel.setBounds(30, 96, 120, 24);
    midiChannelSlider.setBounds(30, 120, 160, 32);

    rootNoteLabel.setBounds(220, 96, 120, 24);
    rootNoteSlider.setBounds(220, 120, 160, 32);

    // ── Row 3: Pitch Envelope (3 sliders) ──────────────────
    const auto rotarySize = 88;
    const auto rotaryGap = 24;
    const auto pitchTotalW = 3 * rotarySize + 2 * rotaryGap;
    const auto pitchX = (w - pitchTotalW) / 2;
    const auto pitchY = 156;

    pitchEnvAmountLabel.setBounds(pitchX, pitchY - 4, rotarySize, 20);
    pitchEnvAmountSlider.setBounds(pitchX, pitchY, rotarySize, rotarySize);

    pitchEnvTimeLabel.setBounds(pitchX + rotarySize + rotaryGap, pitchY - 4, rotarySize, 20);
    pitchEnvTimeSlider.setBounds(pitchX + rotarySize + rotaryGap, pitchY, rotarySize, rotarySize);

    pitchEnvCurveLabel.setBounds(pitchX + 2 * (rotarySize + rotaryGap), pitchY - 4, rotarySize, 20);
    pitchEnvCurveSlider.setBounds(pitchX + 2 * (rotarySize + rotaryGap), pitchY, rotarySize, rotarySize);

    // ── Row 4: ADSR (4 sliders) ────────────────────────────
    const auto adsrTotalW = 4 * rotarySize + 3 * rotaryGap;
    const auto adsrX = (w - adsrTotalW) / 2;
    const auto adsrY = 296;

    ampAttackLabel.setBounds(adsrX, adsrY - 4, rotarySize, 20);
    ampAttackSlider.setBounds(adsrX, adsrY, rotarySize, rotarySize);

    ampDecayLabel.setBounds(adsrX + rotarySize + rotaryGap, adsrY - 4, rotarySize, 20);
    ampDecaySlider.setBounds(adsrX + rotarySize + rotaryGap, adsrY, rotarySize, rotarySize);

    ampSustainLabel.setBounds(adsrX + 2 * (rotarySize + rotaryGap), adsrY - 4, rotarySize, 20);
    ampSustainSlider.setBounds(adsrX + 2 * (rotarySize + rotaryGap), adsrY, rotarySize, rotarySize);

    ampReleaseLabel.setBounds(adsrX + 3 * (rotarySize + rotaryGap), adsrY - 4, rotarySize, 20);
    ampReleaseSlider.setBounds(adsrX + 3 * (rotarySize + rotaryGap), adsrY, rotarySize, rotarySize);

    // ── Row 5: Filter (4 sliders) ──────────────────────────
    const auto filterTotalW = 4 * rotarySize + 3 * rotaryGap;
    const auto filterX = (w - filterTotalW) / 2;
    const auto filterY = 436;

    filterCutoffLabel.setBounds(filterX, filterY - 4, rotarySize, 20);
    filterCutoffSlider.setBounds(filterX, filterY, rotarySize, rotarySize);

    filterResonanceLabel.setBounds(filterX + rotarySize + rotaryGap, filterY - 4, rotarySize, 20);
    filterResonanceSlider.setBounds(filterX + rotarySize + rotaryGap, filterY, rotarySize, rotarySize);

    filterDriveLabel.setBounds(filterX + 2 * (rotarySize + rotaryGap), filterY - 4, rotarySize, 20);
    filterDriveSlider.setBounds(filterX + 2 * (rotarySize + rotaryGap), filterY, rotarySize, rotarySize);

    keyTrackingLabel.setBounds(filterX + 3 * (rotarySize + rotaryGap), filterY - 4, rotarySize, 20);
    keyTrackingSlider.setBounds(filterX + 3 * (rotarySize + rotaryGap), filterY, rotarySize, rotarySize);

    // ── Row 6: Output (2 sliders) ──────────────────────────
    const auto outputTotalW = 2 * rotarySize + rotaryGap;
    const auto outputX = (w - outputTotalW) / 2;
    const auto outputY = 524;

    driveLabel.setBounds(outputX, outputY - 4, rotarySize, 20);
    driveSlider.setBounds(outputX, outputY, rotarySize, rotarySize);

    outputLevelLabel.setBounds(outputX + rotarySize + rotaryGap, outputY - 4, rotarySize, 20);
    outputLevelSlider.setBounds(outputX + rotarySize + rotaryGap, outputY, rotarySize, rotarySize);

    // ── Row 7: MIDI drag button ────────────────────────────
    midiDragButton.setBounds((w - 260) / 2, 600, 260, 34);

    // ── Row 8: Piano keyboard ──────────────────────────────
    pianoKeyboard.setBounds(40, 644, w - 80, 78);
}

juce::AudioProcessorEditor*
VstEngineAudioProcessor::createEditor()
{
    return new VstEngineAudioProcessorEditor(*this);
}
