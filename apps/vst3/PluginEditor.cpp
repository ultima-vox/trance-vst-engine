#include "PluginEditor.h"
#include <ctime>

namespace {

void configureRotarySlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
}

void configureSmallSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::IncDecButtons);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 64, 22);
}

void configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.0f));
}

} // anonymous namespace

VstEngineAudioProcessorEditor::VstEngineAudioProcessorEditor(
    VstEngineAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      midiDragButton(p),
      pianoKeyboard(
          p.keyboardState(),
          juce::MidiKeyboardComponent::horizontalKeyboard),
      seqCallbacks(std::make_unique<SequencerCallbacks>(p)),
      sequencer(std::make_unique<vstengine::ui::StepSequencer>(
          p.sequence(), seqCallbacks.get()))
{
    setSize(920, 876);
    addAndMakeVisible(*sequencer);
    startTimer(33);

    titleLabel.setText(
        "VST ENGINE  /  DARK PSY CORE",
        juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    addAndMakeVisible(midiDragButton);
    addAndMakeVisible(pianoKeyboard);

    // --- MIDI mode / generator settings ---
    midiModeBox.addItem("AUTO", 1);
    midiModeBox.addItem("PIANO ROLL", 2);
    midiModeBox.addItem("GENERATOR", 3);
    midiModeBox.addItem("BOTH", 4);
    addAndMakeVisible(midiModeBox);
    configureSmallSlider(midiChannelSlider);
    configureSmallSlider(rootNoteSlider);
    configureSmallSlider(rngSeedSlider);
    addAndMakeVisible(midiChannelSlider);
    addAndMakeVisible(rootNoteSlider);
    addAndMakeVisible(rngSeedSlider);

    // --- Preset system ---
    addAndMakeVisible(presetBox);
    presetNameEditor.setTextToShowWhenEmpty("preset name", juce::Colours::grey);
    addAndMakeVisible(presetNameEditor);
    for (auto* button : { &presetSaveButton, &presetSaveFullButton,
                          &presetLoadButton, &presetRenameButton,
                          &presetDeleteButton, &presetRefreshButton })
        addAndMakeVisible(button);

    presetSaveButton.onClick = [this] {
        auto* manager = processor.presetManager();
        if (manager == nullptr) return;
        const auto name = presetNameEditor.getText().trim();
        if (name.isEmpty()) return;
        manager->saveSoundPreset(name);
        refreshPresetList();
    };
    presetSaveFullButton.onClick = [this] {
        auto* manager = processor.presetManager();
        if (manager == nullptr) return;
        const auto name = presetNameEditor.getText().trim();
        if (name.isEmpty()) return;
        manager->saveFullPreset(name);
        refreshPresetList();
    };
    presetLoadButton.onClick = [this] {
        auto* manager = processor.presetManager();
        if (manager == nullptr) return;
        const auto itemId = presetBox.getSelectedId();
        if (itemId <= 0) return;
        const auto name = presetBox.getText().trim();
        if (name.isEmpty()) return;
        switch (presetKindForId(itemId)) {
            case PresetKind::factory:   manager->loadFactoryPreset(name); break;
            case PresetKind::userSound: manager->loadSoundPreset(name);   break;
            case PresetKind::userFull:  manager->loadFullPreset(name);    break;
        }
    };
    presetRenameButton.onClick = [this] {
        auto* manager = processor.presetManager();
        if (manager == nullptr) return;
        const auto newName = presetNameEditor.getText().trim();
        const auto currentName = manager->getCurrentPresetName();
        if (newName.isEmpty() || currentName.isEmpty() || newName == currentName)
            return;
        if (manager->renamePreset(currentName, newName))
            refreshPresetList();
    };
    presetDeleteButton.onClick = [this] {
        auto* manager = processor.presetManager();
        if (manager == nullptr) return;
        const auto itemId = presetBox.getSelectedId();
        if (itemId < 2000) return; // only user presets are deletable
        const auto name = presetBox.getText().trim();
        if (name.isEmpty()) return;
        if (manager->deletePreset(name))
            refreshPresetList();
    };
    presetRefreshButton.onClick = [this] { refreshPresetList(); };
    refreshPresetList();

    // --- Sound-shaping rotaries ---
    for (auto* slider : { &filterCutoffSlider, &filterResonanceSlider,
                          &filterDriveSlider, &keyTrackingSlider,
                          &ampAttackSlider, &ampDecaySlider,
                          &ampSustainSlider, &ampReleaseSlider,
                          &pitchEnvAmountSlider, &pitchEnvTimeSlider,
                          &pitchEnvCurveSlider, &driveSlider,
                          &outputLevelSlider }) {
        configureRotarySlider(*slider);
        addAndMakeVisible(slider);
    }

    // --- Labels ---
    configureLabel(presetLabel, "PRESET");
    configureLabel(midiModeLabel, "MIDI SOURCE");
    configureLabel(midiChannelLabel, "GEN MIDI CH");
    configureLabel(rootNoteLabel, "ROOT NOTE");
    configureLabel(rngSeedLabel, "GEN SEED");
    configureLabel(filterGroupLabel, "FILTER");
    configureLabel(ampGroupLabel, "AMP ENVELOPE");
    configureLabel(pitchGroupLabel, "PITCH ENVELOPE");
    configureLabel(outputGroupLabel, "OUTPUT");
    configureLabel(filterCutoffLabel, "CUTOFF");
    configureLabel(filterResonanceLabel, "RESONANCE");
    configureLabel(filterDriveLabel, "FILT DRIVE");
    configureLabel(keyTrackingLabel, "KEY TRACK");
    configureLabel(ampAttackLabel, "ATTACK");
    configureLabel(ampDecayLabel, "DECAY");
    configureLabel(ampSustainLabel, "SUSTAIN");
    configureLabel(ampReleaseLabel, "RELEASE");
    configureLabel(pitchEnvAmountLabel, "P AMOUNT");
    configureLabel(pitchEnvTimeLabel, "P TIME");
    configureLabel(pitchEnvCurveLabel, "P CURVE");
    configureLabel(driveLabel, "DRIVE");
    configureLabel(outputLevelLabel, "OUTPUT");

    for (auto* label : { &presetLabel, &midiModeLabel, &midiChannelLabel,
                         &rootNoteLabel, &rngSeedLabel,
                         &filterGroupLabel, &ampGroupLabel, &pitchGroupLabel,
                         &outputGroupLabel, &filterCutoffLabel,
                         &filterResonanceLabel, &filterDriveLabel,
                         &keyTrackingLabel, &ampAttackLabel, &ampDecayLabel,
                         &ampSustainLabel, &ampReleaseLabel,
                         &pitchEnvAmountLabel, &pitchEnvTimeLabel,
                         &pitchEnvCurveLabel, &driveLabel,
                         &outputLevelLabel })
        addAndMakeVisible(label);

    // --- Attachments ---
    auto attachSlider = [this](juce::Slider& slider, const char* paramID) {
        return std::make_unique<SliderAttachment>(
            processor.parameters(), paramID, slider);
    };

    midiModeAttachment = std::make_unique<ComboBoxAttachment>(
        processor.parameters(), "midiMode", midiModeBox);
    midiChannelAttachment = attachSlider(midiChannelSlider, "midiChannel");
    rootNoteAttachment = attachSlider(rootNoteSlider, "rootNote");
    rngSeedAttachment = attachSlider(rngSeedSlider, "rngSeed");
    filterCutoffAttachment = attachSlider(filterCutoffSlider, "filterCutoff");
    filterResonanceAttachment =
        attachSlider(filterResonanceSlider, "filterResonance");
    filterDriveAttachment = attachSlider(filterDriveSlider, "filterDrive");
    keyTrackingAttachment = attachSlider(keyTrackingSlider, "keyTracking");
    ampAttackAttachment = attachSlider(ampAttackSlider, "ampAttack");
    ampDecayAttachment = attachSlider(ampDecaySlider, "ampDecay");
    ampSustainAttachment = attachSlider(ampSustainSlider, "ampSustain");
    ampReleaseAttachment = attachSlider(ampReleaseSlider, "release");
    pitchEnvAmountAttachment =
        attachSlider(pitchEnvAmountSlider, "pitchEnvAmount");
    pitchEnvTimeAttachment = attachSlider(pitchEnvTimeSlider, "pitchEnvTime");
    pitchEnvCurveAttachment =
        attachSlider(pitchEnvCurveSlider, "pitchEnvCurve");
    driveAttachment = attachSlider(driveSlider, "drive");
    outputLevelAttachment = attachSlider(outputLevelSlider, "outputLevel");
}

void VstEngineAudioProcessorEditor::SequencerCallbacks::onCopy()
{
    clipboard = processor.sequence().copy();
    hasClipboard = true;
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onPaste()
{
    if (hasClipboard)
        processor.sequence().paste(clipboard);
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onRotateLeft() {
    processor.sequence().rotateLeft();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onRotateRight() {
    processor.sequence().rotateRight();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onReverse() {
    processor.sequence().reverse();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onShiftLeft() {
    processor.sequence().shiftLeft();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onShiftRight() {
    processor.sequence().shiftRight();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onTransposeUp() {
    processor.sequence().transpose(1);
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onTransposeDown() {
    processor.sequence().transpose(-1);
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onOctaveUp() {
    processor.sequence().octaveUp();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onOctaveDown() {
    processor.sequence().octaveDown();
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onMutate() {
    processor.sequence().mutateSelected(static_cast<int>(std::time(nullptr)));
}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onClear() {
    processor.sequence().clear();
}

VstEngineAudioProcessorEditor::PresetKind
VstEngineAudioProcessorEditor::presetKindForId (int itemId) const noexcept
{
    if (itemId >= 3000) return PresetKind::userFull;
    if (itemId >= 2000) return PresetKind::userSound;
    return PresetKind::factory;
}

void VstEngineAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    auto* manager = processor.presetManager();
    if (manager == nullptr) return;

    int itemId = 1000;
    for (const auto& name : manager->getFactoryPresetNames())
        presetBox.addItem(name, itemId++);

    presetBox.addSeparator();

    itemId = 2000;
    for (const auto& name : manager->getSoundPresetNames())
        presetBox.addItem(name, itemId++);

    presetBox.addSeparator();

    itemId = 3000;
    for (const auto& name : manager->getFullPresetNames())
        presetBox.addItem(name, itemId++);
}

void VstEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(11, 15, 26));

    const auto bounds = getLocalBounds().toFloat().reduced(22.0f);
    g.setColour(juce::Colour::fromRGB(45, 53, 70));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    g.setColour(juce::Colour::fromRGB(185, 164, 105));
    g.drawLine(24.0f, 262.0f,
               static_cast<float>(getWidth() - 24), 262.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(13.0f);
    g.drawText(
        "AUTO: piano roll wins when MIDI notes are present; otherwise generator.",
        40, 826, getWidth() - 80, 22,
        juce::Justification::centredLeft);
    g.drawText(
        "Generator MIDI is exposed to the host for recording/routing; drag exports a MIDI clip.",
        40, 846, getWidth() - 80, 22,
        juce::Justification::centredLeft);
}

void VstEngineAudioProcessorEditor::resized()
{
    const auto w = getWidth();

    titleLabel.setBounds(34, 26, w - 68, 30);
    sequencer->setBounds(40, 66, w - 80, 260);

    // MIDI source row
    midiModeLabel.setBounds(55, 336, 190, 20);
    midiModeBox.setBounds(55, 360, 190, 28);
    midiChannelLabel.setBounds(280, 336, 130, 20);
    midiChannelSlider.setBounds(295, 360, 100, 28);
    rootNoteLabel.setBounds(470, 336, 130, 20);
    rootNoteSlider.setBounds(485, 360, 100, 28);
    rngSeedLabel.setBounds(660, 336, 150, 20);
    rngSeedSlider.setBounds(675, 360, 150, 28);

    // Preset row
    presetLabel.setBounds(40, 398, 52, 22);
    presetBox.setBounds(96, 398, 170, 24);
    presetNameEditor.setBounds(272, 398, 130, 24);
    presetSaveButton.setBounds(408, 398, 52, 24);
    presetSaveFullButton.setBounds(466, 398, 70, 24);
    presetLoadButton.setBounds(542, 398, 52, 24);
    presetRenameButton.setBounds(600, 398, 52, 24);
    presetDeleteButton.setBounds(658, 398, 52, 24);
    presetRefreshButton.setBounds(716, 398, 60, 24);

    // Parameter grid: 7 columns, 2 rows of rotaries.
    constexpr int numCols = 7;
    constexpr int colW = 116;
    constexpr int rowY1 = 468;
    constexpr int rowY2 = 594;
    const int startX = 40;
    const int colGap = (w - 80 - numCols * colW) / (numCols - 1);

    struct Slot { juce::Slider* slider; juce::Label* label; };
    const Slot row1[] = {
        { &filterCutoffSlider, &filterCutoffLabel },
        { &filterResonanceSlider, &filterResonanceLabel },
        { &filterDriveSlider, &filterDriveLabel },
        { &keyTrackingSlider, &keyTrackingLabel },
        { &ampAttackSlider, &ampAttackLabel },
        { &ampDecaySlider, &ampDecayLabel },
        { &ampSustainSlider, &ampSustainLabel },
    };
    const Slot row2[] = {
        { &ampReleaseSlider, &ampReleaseLabel },
        { &pitchEnvAmountSlider, &pitchEnvAmountLabel },
        { &pitchEnvTimeSlider, &pitchEnvTimeLabel },
        { &pitchEnvCurveSlider, &pitchEnvCurveLabel },
        { &driveSlider, &driveLabel },
        { &outputLevelSlider, &outputLevelLabel },
        { nullptr, nullptr },
    };

    for (int i = 0; i < numCols; ++i) {
        const int x = startX + i * (colW + colGap);
        if (row1[i].slider != nullptr) {
            row1[i].label->setBounds(x, rowY1 - 18, colW, 16);
            row1[i].slider->setBounds(x, rowY1, colW, 96);
        }
        if (row2[i].slider != nullptr) {
            row2[i].label->setBounds(x, rowY2 - 18, colW, 16);
            row2[i].slider->setBounds(x, rowY2, colW, 96);
        }
    }

    // Group headers
    filterGroupLabel.setBounds(startX, 430, colW * 2 + colGap, 18);
    ampGroupLabel.setBounds(startX + 4 * (colW + colGap), 430,
                            colW * 3 + colGap, 18);
    pitchGroupLabel.setBounds(startX + 1 * (colW + colGap), 558,
                              colW * 3 + colGap, 18);
    outputGroupLabel.setBounds(startX + 4 * (colW + colGap), 558,
                               colW * 3 + colGap, 18);

    midiDragButton.setBounds(40, 702, 240, 32);
    pianoKeyboard.setBounds(40, 742, w - 80, 78);
}

juce::AudioProcessorEditor*
VstEngineAudioProcessor::createEditor()
{
    return new VstEngineAudioProcessorEditor(*this);
}

VstEngineAudioProcessorEditor::~VstEngineAudioProcessorEditor()
{
    stopTimer();
}