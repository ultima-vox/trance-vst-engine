#include "PluginEditor.h"
#include "ui/common/UiComponents.h"
#include <ctime>

using Page = vstengine::ui::MainNavigation::Page;
using PresetManager = vstengine::PresetManager;

namespace {
constexpr int instrumentKeyboardWhiteKeys = 43; // C1..C7 inclusive
}

VstEngineAudioProcessorEditor::MidiDragButton::MidiDragButton (
    juce::String text, std::function<juce::File()> create)
    : juce::TextButton (std::move (text)), createFile (std::move (create))
{
    vstengine::ui::styleButton (*this);
}

void VstEngineAudioProcessorEditor::MidiDragButton::mouseDown (
    const juce::MouseEvent& event)
{
    started = false;
    juce::TextButton::mouseDown (event);
}

void VstEngineAudioProcessorEditor::MidiDragButton::mouseDrag (
    const juce::MouseEvent& event)
{
    juce::TextButton::mouseDrag (event);
    if (started || event.getDistanceFromDragStart() < 6 || ! createFile)
        return;
    const auto file = createFile();
    if (! file.existsAsFile())
        return;
    started = true;
    juce::StringArray files;
    files.add (file.getFullPathName());
    juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false,
                                                                this);
}

VstEngineAudioProcessorEditor::InstrumentPage::InstrumentPage (
    juce::AudioProcessorValueTreeState& state,
    juce::MidiKeyboardState& keyboardState,
    PresetManager& manager,
    PresetManager::SoundEngine soundEngine,
    juce::String partName,
    const char* channelParameter,
    const char* muteParameter,
    const char* soloParameter,
    const char* lockParameter,
    const char* levelParameter,
    const char* panParameter,
    std::unique_ptr<juce::Component> enginePanel,
    std::function<juce::File()> createMidiFile,
    std::function<void()> onStateChanged)
    : presetManager (manager), engine (soundEngine),
      stateChanged (std::move (onStateChanged)), panel (std::move (enginePanel)),
      keyboard (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      dragButton ("DRAG " + partName + " MIDI", std::move (createMidiFile))
{
    title.setText (partName, juce::dontSendNotification);
    presetLabel.setText ("Preset", juce::dontSendNotification);
    channelLabel.setText ("CH", juce::dontSendNotification);
    levelLabel.setText ("Level", juce::dontSendNotification);
    panLabel.setText ("Pan", juce::dontSendNotification);
    keyboardLabel.setText (partName + " KEYBOARD / PIANO",
                           juce::dontSendNotification);
    status.setText ("Audition and MIDI export route only to " + partName + ".",
                    juce::dontSendNotification);
    vstengine::ui::styleLabel (title, 17, juce::Justification::centredLeft,
                               vstengine::ui::colours::primary);
    for (auto* label : { &presetLabel, &channelLabel, &levelLabel, &panLabel,
                         &keyboardLabel, &status })
        vstengine::ui::styleLabel (*label, 12, juce::Justification::centredLeft,
                                   label == &keyboardLabel
                                       ? vstengine::ui::colours::primary
                                       : vstengine::ui::colours::mutedText);

    preset.setEditableText (true);
    preset.setTextWhenNothingSelected ("Select preset");
    preset.onChange = [this] { loadSelectedPreset(); };
    for (auto* button : { &previous, &next, &save, &saveAs, &mute, &solo, &lock }) {
        vstengine::ui::styleButton (*button);
        addAndMakeVisible (*button);
    }
    previous.onClick = [this] { selectAdjacentPreset (-1); };
    next.onClick = [this] { selectAdjacentPreset (1); };
    save.onClick = [this] { savePreset (false); };
    saveAs.onClick = [this] { savePreset (true); };
    for (auto* button : { &mute, &solo, &lock })
        button->setClickingTogglesState (true);

    channel.setSliderStyle (juce::Slider::LinearHorizontal);
    channel.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 22);
    level.setSliderStyle (juce::Slider::LinearHorizontal);
    level.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    pan.setSliderStyle (juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    keyboard.setAvailableRange (24, 96);
    keyboard.setOctaveForMiddleC (3);

    const std::array<juce::Component*, 14> components {
        &title, &presetLabel, &preset, &channelLabel, &channel, &levelLabel, &level,
        &panLabel, &pan, &keyboardLabel, &keyboard, &dragButton, &status, panel.get()
    };
    for (auto* component : components)
        addAndMakeVisible (component);

    sliderAttachments[0] = std::make_unique<SliderAttachment> (state, channelParameter,
                                                               channel);
    sliderAttachments[1] = std::make_unique<SliderAttachment> (state, levelParameter,
                                                               level);
    sliderAttachments[2] = std::make_unique<SliderAttachment> (state, panParameter,
                                                               pan);
    buttonAttachments[0] = std::make_unique<ButtonAttachment> (state, muteParameter,
                                                               mute);
    buttonAttachments[1] = std::make_unique<ButtonAttachment> (state, soloParameter,
                                                               solo);
    buttonAttachments[2] = std::make_unique<ButtonAttachment> (state, lockParameter,
                                                               lock);
    refreshPresets();
}

void VstEngineAudioProcessorEditor::InstrumentPage::paint (juce::Graphics& g)
{
    g.fillAll (vstengine::ui::colours::background);
    g.setColour (vstengine::ui::colours::panel);
    g.fillRoundedRectangle (8.0f, 6.0f, static_cast<float> (getWidth() - 16), 48.0f,
                            8.0f);
    g.fillRoundedRectangle (8.0f, static_cast<float> (getHeight() - 120),
                            static_cast<float> (getWidth() - 16), 112.0f, 8.0f);
}

void VstEngineAudioProcessorEditor::InstrumentPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (40);
    title.setBounds (bar.removeFromLeft (62));
    presetLabel.setBounds (bar.removeFromLeft (44));
    preset.setBounds (bar.removeFromLeft (170).reduced (2));
    previous.setBounds (bar.removeFromLeft (34).reduced (2));
    next.setBounds (bar.removeFromLeft (34).reduced (2));
    save.setBounds (bar.removeFromLeft (56).reduced (2));
    saveAs.setBounds (bar.removeFromLeft (72).reduced (2));
    channelLabel.setBounds (bar.removeFromLeft (24));
    channel.setBounds (bar.removeFromLeft (88).reduced (2));
    mute.setBounds (bar.removeFromLeft (34).reduced (2));
    solo.setBounds (bar.removeFromLeft (34).reduced (2));
    lock.setBounds (bar.removeFromLeft (50).reduced (2));
    levelLabel.setBounds (bar.removeFromLeft (36));
    level.setBounds (bar.removeFromLeft (108).reduced (2));
    panLabel.setBounds (bar.removeFromLeft (28));
    pan.setBounds (bar.reduced (2));

    area.removeFromTop (8);
    auto audition = area.removeFromBottom (104);
    auto auditionHeader = audition.removeFromTop (26);
    keyboardLabel.setBounds (auditionHeader.removeFromLeft (190));
    dragButton.setBounds (auditionHeader.removeFromLeft (170).reduced (2));
    status.setBounds (auditionHeader.reduced (8, 0));
    const auto keyboardBounds = audition.reduced (2);
    keyboard.setBounds (keyboardBounds);
    keyboard.setKeyWidth (static_cast<float> (keyboardBounds.getWidth())
                          / static_cast<float> (instrumentKeyboardWhiteKeys));
    keyboard.setLowestVisibleKey (24);
    panel->setBounds (area);
}

void VstEngineAudioProcessorEditor::InstrumentPage::refreshPresets()
{
    const auto oldText = preset.getText();
    availablePresets.clear();
    preset.clear (juce::dontSendNotification);
    for (const auto& entry : presetManager.getPresets())
        if (entry.kind == PresetManager::PresetKind::sound && entry.engine == engine) {
            availablePresets.add (entry);
            preset.addItem (entry.name, availablePresets.size());
        }
    int index = -1;
    for (int i = 0; i < availablePresets.size(); ++i)
        if (availablePresets.getReference (i).name == oldText) {
            index = i;
            break;
        }
    if (index >= 0)
        preset.setSelectedItemIndex (index, juce::dontSendNotification);
    else if (! availablePresets.isEmpty())
        preset.setSelectedItemIndex (0, juce::dontSendNotification);
}

void VstEngineAudioProcessorEditor::InstrumentPage::loadSelectedPreset()
{
    const int index = preset.getSelectedItemIndex();
    if (index < 0 || index >= availablePresets.size())
        return;
    const auto result = presetManager.loadPreset (availablePresets.getReference (index));
    status.setText (result.wasOk() ? "Loaded " + preset.getText() : result.message,
                    juce::dontSendNotification);
    if (result && stateChanged)
        stateChanged();
}

void VstEngineAudioProcessorEditor::InstrumentPage::selectAdjacentPreset (int delta)
{
    if (availablePresets.isEmpty())
        return;
    const int current = juce::jmax (0, preset.getSelectedItemIndex());
    preset.setSelectedItemIndex (juce::jlimit (0, availablePresets.size() - 1,
                                               current + delta),
                                 juce::sendNotificationSync);
}

void VstEngineAudioProcessorEditor::InstrumentPage::savePreset (bool saveAsCopy)
{
    auto name = preset.getText().trim();
    if (name.isEmpty())
        name = "Bass User";
    if (saveAsCopy)
        name += " Copy";
    const auto result = presetManager.saveSoundPreset (name, engine);
    status.setText (result.wasOk() ? "Saved " + name : result.message,
                    juce::dontSendNotification);
    if (result) {
        preset.setText (name, juce::dontSendNotification);
        refreshPresets();
    }
}

VstEngineAudioProcessorEditor::SequencerCallbacks::SequencerCallbacks (
    VstEngineAudioProcessor& p, int index)
    : processor (p), partIndex (index)
{
}

vstengine::sequence::Sequence&
VstEngineAudioProcessorEditor::SequencerCallbacks::model() const
{
    return processor.partSequence (partIndex);
}

bool VstEngineAudioProcessorEditor::SequencerCallbacks::editable() const
{
    return ! processor.isPartLocked (partIndex);
}

void VstEngineAudioProcessorEditor::SequencerCallbacks::onCopy() { model().copyTo (clipboard); hasClipboard = true; }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onPaste() { if (editable() && hasClipboard) model().paste (clipboard); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onRotateLeft() { if (editable()) model().rotateLeft(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onRotateRight() { if (editable()) model().rotateRight(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onReverse() { if (editable()) model().reverse(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onShiftLeft() { if (editable()) model().shiftLeft(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onShiftRight() { if (editable()) model().shiftRight(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onTransposeUp() { if (editable()) model().transpose (1); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onTransposeDown() { if (editable()) model().transpose (-1); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onOctaveUp() { if (editable()) model().octaveUp(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onOctaveDown() { if (editable()) model().octaveDown(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onMutate() { if (editable()) model().mutateSelected (static_cast<int> (std::time (nullptr))); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onClear() { if (editable()) model().clearSelected(); }
void VstEngineAudioProcessorEditor::SequencerCallbacks::onSequenceChanged() { processor.publishPartSequenceForAudio (partIndex); }

VstEngineAudioProcessorEditor::SequencerPage::SequencerPage (
    VstEngineAudioProcessor& processor)
    : callbacks (processor, 0), sequence (processor.partSequence (0), &callbacks)
{
    vstengine::ui::styleLabel (title, 13, juce::Justification::centredLeft,
                               vstengine::ui::colours::primary);
    addAndMakeVisible (title);
    addAndMakeVisible (sequence);
}

void VstEngineAudioProcessorEditor::SequencerPage::paint (juce::Graphics& g)
{
    g.fillAll (vstengine::ui::colours::background);
    g.setColour (vstengine::ui::colours::panel);
    g.fillRoundedRectangle (8.0f, 6.0f, static_cast<float> (getWidth() - 16), 42.0f,
                            8.0f);
}

void VstEngineAudioProcessorEditor::SequencerPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto top = area.removeFromTop (34);
    title.setBounds (top);
    area.removeFromTop (8);
    sequence.setBounds (area);
}

void VstEngineAudioProcessorEditor::SequencerPage::setPlayHeadPosition (int step)
{
    sequence.setPlayHeadPosition (step);
}

void VstEngineAudioProcessorEditor::SequencerPage::setLocked (const bool locked)
{
    sequence.setEnabled (! locked);
}

void VstEngineAudioProcessorEditor::SequencerPage::refreshFromModels()
{
    sequence.refreshFromModel();
}

VstEngineAudioProcessorEditor::SettingsPage::SettingsPage (
    juce::AudioProcessorValueTreeState& state, std::function<void()> panic)
{
    for (auto* titleLabel : { &midiTitle, &generatorTitle, &systemTitle })
        vstengine::ui::styleLabel (*titleLabel, 13, juce::Justification::centredLeft,
                                   vstengine::ui::colours::primary);
    for (auto* label : { &midiSourceLabel, &seedLabel, &syncLabel, &syncValue, &status })
        vstengine::ui::styleLabel (*label, 12, juce::Justification::centredLeft,
                                   vstengine::ui::colours::mutedText);
    midiMode.addItemList ({ "AUTO", "PIANO ROLL", "GENERATOR", "BOTH" }, 1);
    seed.setSliderStyle (juce::Slider::LinearHorizontal);
    seed.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 22);
    vstengine::ui::styleButton (panicButton);
    panicButton.setColour (juce::TextButton::buttonColourId,
                           vstengine::ui::colours::warning.darker (0.45f));
    panicButton.onClick = std::move (panic);
    const std::array<juce::Component*, 11> components {
        &midiTitle, &midiSourceLabel, &midiMode, &generatorTitle, &seedLabel,
        &seed, &syncLabel, &syncValue, &systemTitle, &panicButton, &status
    };
    for (auto* component : components)
        addAndMakeVisible (component);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, "midiMode", midiMode);
    seedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "rngSeed", seed);
}

void VstEngineAudioProcessorEditor::SettingsPage::paint (juce::Graphics& g)
{
    g.fillAll (vstengine::ui::colours::background);
    g.setColour (vstengine::ui::colours::panel);
    const auto width = (getWidth() - 40) / 3;
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (static_cast<float> (12 + i * (width + 8)), 12.0f,
                                static_cast<float> (width),
                                static_cast<float> (getHeight() - 24), 8.0f);
}

void VstEngineAudioProcessorEditor::SettingsPage::resized()
{
    auto area = getLocalBounds().reduced (24);
    const int width = (area.getWidth() - 16) / 3;
    auto midi = area.removeFromLeft (width).reduced (12);
    area.removeFromLeft (8);
    auto generator = area.removeFromLeft (width).reduced (12);
    area.removeFromLeft (8);
    auto system = area.reduced (12);
    midiTitle.setBounds (midi.removeFromTop (28));
    midiSourceLabel.setBounds (midi.removeFromTop (22));
    midiMode.setBounds (midi.removeFromTop (36).reduced (2));
    generatorTitle.setBounds (generator.removeFromTop (28));
    seedLabel.setBounds (generator.removeFromTop (22));
    seed.setBounds (generator.removeFromTop (40));
    generator.removeFromTop (12);
    syncLabel.setBounds (generator.removeFromTop (22));
    syncValue.setBounds (generator.removeFromTop (30));
    systemTitle.setBounds (system.removeFromTop (28));
    panicButton.setBounds (system.removeFromTop (44).reduced (2));
    status.setBounds (system.removeFromTop (48));
}

VstEngineAudioProcessorEditor::VstEngineAudioProcessorEditor (
    VstEngineAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      header (p.parameters(), { [this] { selectAdjacentPreset (-1); },
                                [this] { selectAdjacentPreset (1); },
                                [this] { showPage (Page::presets); },
                                [&p] { p.requestPanic(); } }),
      bass (p.parameters(), p.bassKeyboardState(), *p.presetManager(),
            PresetManager::SoundEngine::bass, "BASS", "midiChannel", "bassMute",
            "bassSolo", "bassLock", "bassLevel", "bassPan",
            std::make_unique<vstengine::ui::BassPanel> (p.parameters()),
            [&p] { return p.createPartMidiFile (0); }, [this] { refreshPartUi(); }),
      sequence (p),
      presets (*p.presetManager(), [this] { refreshPartUi(); }),
      settings (p.parameters(), [&p] { p.requestPanic(); }),
      pages { &bass, &sequence, &presets, &settings }
{
    addAndMakeVisible (header);
    addAndMakeVisible (navigation);
    for (auto* page : pages)
        addChildComponent (page);
    navigation.onPageChanged = [this] (Page page) { showPage (page); };
    setResizable (true, true);
    setResizeLimits (1040, 680, 1600, 1000);
    setSize (1180, 760);
    showPage (Page::bass);
    startTimerHz (30);
}

VstEngineAudioProcessorEditor::~VstEngineAudioProcessorEditor() { stopTimer(); }

void VstEngineAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (vstengine::ui::colours::background);
}

void VstEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds (area.removeFromTop (58));
    navigation.setBounds (area.removeFromTop (44).reduced (10, 4));
    for (auto* page : pages)
        page->setBounds (area.reduced (10, 6));
}

void VstEngineAudioProcessorEditor::showPage (Page page)
{
    const auto index = static_cast<size_t> (page);
    if (index >= pages.size())
        return;
    for (size_t i = 0; i < pages.size(); ++i)
        pages[i]->setVisible (i == index);
    if (navigation.getCurrentPage() != page)
        navigation.setCurrentPage (page);
}

void VstEngineAudioProcessorEditor::showPageForTesting (Page page) { showPage (page); }
Page VstEngineAudioProcessorEditor::currentPageForTesting() const noexcept { return navigation.getCurrentPage(); }

float VstEngineAudioProcessorEditor::keyboardKeyWidthForTesting (
    const Page page) const noexcept
{
    juce::ignoreUnused (page);
    return bass.keyboardKeyWidthForTesting();
}

int VstEngineAudioProcessorEditor::keyboardComponentWidthForTesting (
    const Page page) const noexcept
{
    juce::ignoreUnused (page);
    return bass.keyboardComponentWidthForTesting();
}

void VstEngineAudioProcessorEditor::timerCallback()
{
    processor.publishPartSequenceForAudio (0);
    sequence.setPlayHeadPosition (processor.getPartPlayHeadStep (0));
    sequence.setLocked (processor.isPartLocked (0));
    const bool playing = processor.getPartPlayHeadStep (0) >= 0;
    header.setTransportActive (playing);
    if (auto* manager = processor.presetManager())
        header.setPresetName (manager->getCurrentPresetName());
}

void VstEngineAudioProcessorEditor::refreshPartUi()
{
    sequence.refreshFromModels();
    processor.publishPartSequenceForAudio (0);
    bass.refreshPresets();
    presets.refresh();
}

void VstEngineAudioProcessorEditor::selectAdjacentPreset (int delta)
{
    auto* manager = processor.presetManager();
    if (manager == nullptr)
        return;
    const auto entries = manager->getPresets();
    if (entries.isEmpty())
        return;
    int current = 0;
    const auto name = manager->getCurrentPresetName();
    for (int i = 0; i < entries.size(); ++i)
        if (entries.getReference (i).name == name) {
            current = i;
            break;
        }
    const int nextIndex = juce::jlimit (0, entries.size() - 1, current + delta);
    if (manager->loadPreset (entries.getReference (nextIndex)))
        refreshPartUi();
}

juce::AudioProcessorEditor* VstEngineAudioProcessor::createEditor()
{
    return new VstEngineAudioProcessorEditor (*this);
}
