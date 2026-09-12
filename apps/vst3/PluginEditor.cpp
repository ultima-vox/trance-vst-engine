#include "PluginEditor.h"
#include "instrument/HostParameterSchema.h"
#include "ui/common/UiComponents.h"
#include <array>
#include <cstdio>

using Page = vstengine::ui::MainNavigation::Page;
namespace {
std::string slotId(std::size_t slot, std::string_view suffix)
{
    char prefix[16] {};
    std::snprintf(prefix, sizeof(prefix), "slot%02zu", slot + 1);
    return std::string(prefix) + std::string(suffix);
}
void knob(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 18);
}
juce::String resolutionText(vstengine::instrument::ResolutionStatus status)
{
    using S = vstengine::instrument::ResolutionStatus;
    switch (status) {
        case S::resolved: return "Ready";
        case S::missingModule: return "Missing";
        case S::incompatibleAbi: return "Incompatible ABI";
        case S::incompatibleSchema: return "Incompatible state";
        case S::invalidDescriptor: return "Invalid descriptor";
        case S::budgetExceeded: return "Budget exceeded";
        case S::constructionFailed: return "Load failed";
    }
    return "Unknown";
}
} // namespace

VstEngineAudioProcessorEditor::RackPage::RackPage(VstEngineAudioProcessor& p)
    : processor(p),
      keyboard(p.keyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    vstengine::ui::styleLabel(title, 15, juce::Justification::centredLeft,
                              vstengine::ui::colours::primary);
    title.setText("GENERIC INSTRUMENT RACK", juce::dontSendNotification);
    vstengine::ui::styleLabel(routingTitle, 12, juce::Justification::centredLeft,
                              vstengine::ui::colours::mutedText);
    routingTitle.setText("ROUTING / ZONES / MIX", juce::dontSendNotification);
    vstengine::ui::styleLabel(macroTitle, 12, juce::Justification::centredLeft,
                              vstengine::ui::colours::mutedText);
    macroTitle.setText("CUBASE AUTOMATION MACROS", juce::dontSendNotification);
    vstengine::ui::styleLabel(status, 12, juce::Justification::centredLeft,
                              vstengine::ui::colours::mutedText);
    const std::array<juce::Component*, 24> components {
        &title, &routingTitle, &macroTitle, &status, &instrument, &midiIn,
        &layer, &enabled, &mute, &solo, &locked, &keyLow, &keyHigh,
        &velocityLow, &velocityHigh, &transpose, &level, &pan, &keyboard,
        &swap, &move, &layerAction, &soundPreset, &patternProfile
    };
    for (auto* component : components)
        addAndMakeVisible(component);
    midiIn.addItem("OFF", 1);
    for (int channel = 1; channel <= 16; ++channel)
        midiIn.addItem("CH" + juce::String(channel), channel + 1);
    int item = 1;
    for (const auto* descriptor : processor.availableInstruments())
        instrument.addItem(descriptor->name, item++);
    for (auto* slider : { &keyLow, &keyHigh, &velocityLow, &velocityHigh,
                          &transpose, &level, &pan }) knob(*slider);
    for (std::size_t i = 0; i < macros.size(); ++i) {
        knob(macros[i]);
        vstengine::ui::styleLabel(macroLabels[i], 10,
            juce::Justification::centred, vstengine::ui::colours::mutedText);
        addAndMakeVisible(macros[i]); addAndMakeVisible(macroLabels[i]);
    }
    for (std::size_t i = 0; i < slots.size(); ++i) {
        vstengine::ui::styleButton(slots[i]);
        slots[i].onClick = [this, i] { select(i); };
        addAndMakeVisible(slots[i]);
    }
    for (auto* button : { &swap, &move, &layerAction })
        vstengine::ui::styleButton(*button);
    swap.onClick = [this] { applyChannel(VstEngineAudioProcessor::ChannelConflictAction::swap); };
    move.onClick = [this] { applyChannel(VstEngineAudioProcessor::ChannelConflictAction::move); };
    layerAction.onClick = [this] { applyChannel(VstEngineAudioProcessor::ChannelConflictAction::layer); };
    midiIn.onChange = [this] {
        pendingChannel = midiIn.getSelectedId() - 1;
        applyChannel(VstEngineAudioProcessor::ChannelConflictAction::reject);
    };
    instrument.onChange = [this] {
        const int index = instrument.getSelectedItemIndex();
        const auto descriptors = processor.availableInstruments();
        if (index < 0 || index >= static_cast<int>(descriptors.size())) return;
        juce::String message;
        processor.loadSlotInstrument(selected, descriptors[static_cast<std::size_t>(index)]->id,
                                     message);
        status.setText(message, juce::dontSendNotification);
        refresh();
    };
    soundPreset.onChange = [this] {
        const auto index = soundPreset.getSelectedItemIndex();
        if (index < 0 || index >= static_cast<int>(soundPresetIds.size())) return;
        juce::String message;
        processor.applySelectedSoundPreset(
            soundPresetIds[static_cast<std::size_t>(index)], message);
        status.setText(message.isEmpty() ? "Sound preset loaded" : message,
                       juce::dontSendNotification);
    };
    patternProfile.onChange = [this] {
        const auto index = patternProfile.getSelectedItemIndex();
        if (index < 0 || index >= static_cast<int>(patternProfileIds.size())) return;
        juce::String message;
        processor.generateSelectedPattern(
            patternProfileIds[static_cast<std::size_t>(index)], message);
        status.setText(message.isEmpty() ? "Pattern generated" : message,
                       juce::dontSendNotification);
    };
    select(0);
}

void VstEngineAudioProcessorEditor::RackPage::select(std::size_t index)
{
    selected = juce::jlimit<std::size_t>(0, slots.size() - 1, index);
    processor.selectSlot(selected);
    for (std::size_t i = 0; i < slots.size(); ++i)
        slots[i].setToggleState(i == selected, juce::dontSendNotification);
    bindSelectedSlot();
}

void VstEngineAudioProcessorEditor::RackPage::applyChannel(
    VstEngineAudioProcessor::ChannelConflictAction action)
{
    juce::String message;
    if (!processor.assignSlotChannel(selected, pendingChannel, action, message)) {
        status.setText(message, juce::dontSendNotification);
        midiIn.setSelectedId(static_cast<int>(
            processor.instrumentRack().state()[selected].routing.channel) + 1,
            juce::dontSendNotification);
        return;
    }
    status.setText("Routing updated", juce::dontSendNotification);
}

void VstEngineAudioProcessorEditor::RackPage::bindSelectedSlot()
{
    sliderAttachments.clear(); buttonAttachments.clear();
    const auto& state = processor.instrumentRack().state()[selected];
    int descriptorIndex = 0;
    const auto descriptors = processor.availableInstruments();
    for (std::size_t i = 0; i < descriptors.size(); ++i)
        if (descriptors[i]->id == state.instrumentId)
            descriptorIndex = static_cast<int>(i + 1);
    instrument.setSelectedId(descriptorIndex, juce::dontSendNotification);
    soundPreset.clear(juce::dontSendNotification);
    patternProfile.clear(juce::dontSendNotification);
    soundPresetIds.clear(); patternProfileIds.clear();
    for (const auto& content : processor.selectedContent()) {
        if (content.kind == vstengine::instrument::ContentKind::soundPreset) {
            soundPresetIds.push_back(content.id);
            soundPreset.addItem(content.name,
                static_cast<int>(soundPresetIds.size()));
        } else if (content.kind
                   == vstengine::instrument::ContentKind::generatorProfile) {
            patternProfileIds.push_back(content.id);
            patternProfile.addItem(content.name,
                static_cast<int>(patternProfileIds.size()));
        }
    }
    soundPreset.setTextWhenNothingSelected("Sound preset");
    patternProfile.setTextWhenNothingSelected("Generate pattern");
    pendingChannel = state.routing.mode == vstengine::rack::RouteMode::off
        ? 0 : state.routing.channel;
    midiIn.setSelectedId(pendingChannel + 1, juce::dontSendNotification);
    auto& apvts = processor.parameters();
    auto attachSlider = [&](juce::Slider& slider, std::string_view suffix) {
        sliderAttachments.push_back(std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, slotId(selected, suffix), slider));
    };
    auto attachButton = [&](juce::Button& button, std::string_view suffix) {
        buttonAttachments.push_back(std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, slotId(selected, suffix), button));
    };
    attachButton(layer, "Layer"); attachButton(enabled, "Enabled");
    attachButton(mute, "Mute"); attachButton(solo, "Solo");
    attachButton(locked, "Lock");
    attachSlider(keyLow, "KeyLow"); attachSlider(keyHigh, "KeyHigh");
    attachSlider(velocityLow, "VelocityLow");
    attachSlider(velocityHigh, "VelocityHigh"); attachSlider(transpose, "Transpose");
    attachSlider(level, "Level"); attachSlider(pan, "Pan");
    for (std::size_t macro = 0; macro < macros.size(); ++macro) {
        sliderAttachments.push_back(std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,
                vstengine::instrument::hostparams::macroId(selected, macro), macros[macro]));
        juce::String label(vstengine::instrument::hostparams::macroLabels[macro].data());
        if (const auto* descriptor = processor.instrumentRack().descriptor(selected))
            for (const auto& parameter : descriptor->parameters)
                if (parameter.preferredMacro == static_cast<std::int8_t>(macro))
                    label = parameter.name;
        macroLabels[macro].setText(label, juce::dontSendNotification);
    }
}

void VstEngineAudioProcessorEditor::RackPage::refresh()
{
    const auto& states = processor.instrumentRack().state();
    const auto& runtime = processor.instrumentRack().runtimeState();
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto* descriptor = processor.instrumentRack().descriptor(i);
        const auto name = descriptor ? juce::String(descriptor->name)
                                     : states[i].instrumentId.empty()
                                         ? "Empty" : juce::String(states[i].instrumentId);
        const auto route = states[i].routing.mode == vstengine::rack::RouteMode::off
            ? "OFF" : "CH" + juce::String(states[i].routing.channel);
        slots[i].setButtonText(juce::String(static_cast<int>(i + 1)).paddedLeft('0', 2)
            + "  " + name + "  " + route);
    }
    status.setText("SlotId " + juce::String(states[selected].slotId)
        + " | " + resolutionText(runtime[selected].resolution)
        + " | drops " + juce::String(runtime[selected].droppedMidiEvents),
        juce::dontSendNotification);
}

void VstEngineAudioProcessorEditor::RackPage::paint(juce::Graphics& g)
{
    g.fillAll(vstengine::ui::colours::background);
    g.setColour(vstengine::ui::colours::panel);
    g.fillRoundedRectangle(8, 8, 348.0f, static_cast<float>(getHeight() - 16), 8);
    g.fillRoundedRectangle(364, 8, static_cast<float>(getWidth() - 372),
                           static_cast<float>(getHeight() - 16), 8);
}

void VstEngineAudioProcessorEditor::RackPage::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto list = area.removeFromLeft(332);
    title.setBounds(list.removeFromTop(28));
    for (auto& slot : slots) slot.setBounds(list.removeFromTop(31).reduced(1));
    area.removeFromLeft(24);
    auto top = area.removeFromTop(38);
    instrument.setBounds(top.removeFromLeft(220).reduced(2));
    midiIn.setBounds(top.removeFromLeft(82).reduced(2));
    swap.setBounds(top.removeFromLeft(64).reduced(2));
    move.setBounds(top.removeFromLeft(64).reduced(2));
    layerAction.setBounds(top.removeFromLeft(70).reduced(2));
    auto content = area.removeFromTop(34);
    soundPreset.setBounds(content.removeFromLeft(220).reduced(2));
    patternProfile.setBounds(content.removeFromLeft(220).reduced(2));
    routingTitle.setBounds(area.removeFromTop(22));
    auto toggles = area.removeFromTop(28);
    for (auto* button : { &layer, &enabled, &mute, &solo, &locked })
        button->setBounds(toggles.removeFromLeft(82));
    auto controls = area.removeFromTop(96);
    for (auto* slider : { &keyLow, &keyHigh, &velocityLow, &velocityHigh,
                          &transpose, &level, &pan })
        slider->setBounds(controls.removeFromLeft(76));
    macroTitle.setBounds(area.removeFromTop(22));
    auto macroArea = area.removeFromTop(105);
    const int width = juce::jmax(60, macroArea.getWidth() / 8);
    for (std::size_t i = 0; i < macros.size(); ++i) {
        auto cell = macroArea.removeFromLeft(width);
        macroLabels[i].setBounds(cell.removeFromTop(20));
        macros[i].setBounds(cell);
    }
    area.removeFromTop(8);
    keyboard.setKeyWidth(static_cast<float>(area.getWidth()) / 43.0f);
    keyboard.setAvailableRange(24, 96);
    keyboard.setBounds(area.removeFromTop(72));
    status.setBounds(area.removeFromTop(26));
}

void VstEngineAudioProcessorEditor::SequenceCallbacks::onCopy(){processor.sequence().copyTo(clipboard);copied=true;}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onPaste(){if(copied)processor.sequence().paste(clipboard);}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onRotateLeft(){processor.sequence().rotateLeft();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onRotateRight(){processor.sequence().rotateRight();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onReverse(){processor.sequence().reverse();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onShiftLeft(){processor.sequence().shiftLeft();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onShiftRight(){processor.sequence().shiftRight();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onTransposeUp(){processor.sequence().transpose(1);}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onTransposeDown(){processor.sequence().transpose(-1);}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onOctaveUp(){processor.sequence().octaveUp();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onOctaveDown(){processor.sequence().octaveDown();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onMutate()
{
    juce::String diagnostic;
    (void) processor.mutateSelectedPattern(true, diagnostic);
}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onClear(){processor.sequence().clearSelected();}
void VstEngineAudioProcessorEditor::SequenceCallbacks::onSequenceChanged(){processor.publishSequenceForAudio();}

VstEngineAudioProcessorEditor::SequencePage::SequencePage(VstEngineAudioProcessor& p)
    : processor(p), callbacks(p), sequencer(p.sequence(), &callbacks)
{
    vstengine::ui::styleLabel(status, 12, juce::Justification::centredLeft,
                              vstengine::ui::colours::mutedText);
    addAndMakeVisible(sequencer); addAndMakeVisible(status);
}
void VstEngineAudioProcessorEditor::SequencePage::paint(juce::Graphics& g){g.fillAll(vstengine::ui::colours::background);}
void VstEngineAudioProcessorEditor::SequencePage::resized(){auto a=getLocalBounds().reduced(12);status.setBounds(a.removeFromTop(28));sequencer.setBounds(a);}
void VstEngineAudioProcessorEditor::SequencePage::setPlayHead(int step){sequencer.setPlayHeadPosition(step);}
void VstEngineAudioProcessorEditor::SequencePage::refresh(){
    const auto* descriptor=processor.instrumentRack().descriptor(processor.selectedSlotIndex());
    const bool supported=descriptor && (descriptor->capabilities & vstengine::instrument::Capability::sequence)!=0;
    sequencer.setSequence(processor.sequence());
    sequencer.setSlideEnabled((processor.selectedSequenceFieldMask()
        & VOX_SEQUENCE_SLIDE) != 0);
    sequencer.setEnabled(supported);sequencer.refreshFromModel();
    status.setText(supported?"Selected instrument sequence":"Selected instrument has no Sequence capability",juce::dontSendNotification);
}

VstEngineAudioProcessorEditor::SettingsPage::SettingsPage(VstEngineAudioProcessor& p)
{
    title.setText("HOST / GENERATION",juce::dontSendNotification);description.setText("Deterministic seed; Panic resets all Rack slots.",juce::dontSendNotification);
    vstengine::ui::styleLabel(title,15,juce::Justification::centredLeft,vstengine::ui::colours::primary);vstengine::ui::styleLabel(description,12,juce::Justification::centredLeft,vstengine::ui::colours::mutedText);vstengine::ui::styleLabel(generationStatus,12,juce::Justification::centredLeft,vstengine::ui::colours::status);
    midiMode.addItemList({"AUTO","PIANO ROLL","GENERATOR","BOTH"},1);knob(seed);for(auto*b:{&panic,&generateAll,&mutateAll})vstengine::ui::styleButton(*b);panic.onClick=[&p]{p.requestPanic();};
    generateAll.onClick=[this,&p]{juce::String d;p.generateAllPatterns(d);generationStatus.setText(d,juce::dontSendNotification);};
    mutateAll.onClick=[this,&p]{juce::String d;p.mutateAllPatterns(d);generationStatus.setText(d,juce::dontSendNotification);};
    const std::array<juce::Component*, 8> components {
        &title, &description, &midiMode, &seed, &panic, &generateAll,
        &mutateAll, &generationStatus
    };
    for (auto* component : components) addAndMakeVisible(component);
    modeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters(),"midiMode",midiMode);
    seedAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters(),"rngSeed",seed);
}
void VstEngineAudioProcessorEditor::SettingsPage::paint(juce::Graphics&g){g.fillAll(vstengine::ui::colours::background);}
void VstEngineAudioProcessorEditor::SettingsPage::resized(){auto a=getLocalBounds().reduced(28);title.setBounds(a.removeFromTop(32));description.setBounds(a.removeFromTop(28));midiMode.setBounds(a.removeFromTop(38).removeFromLeft(220));seed.setBounds(a.removeFromTop(100).removeFromLeft(120));auto actions=a.removeFromTop(42);generateAll.setBounds(actions.removeFromLeft(150).reduced(2));mutateAll.setBounds(actions.removeFromLeft(150).reduced(2));panic.setBounds(actions.removeFromLeft(180).reduced(2));generationStatus.setBounds(a.removeFromTop(30));}

VstEngineAudioProcessorEditor::VstEngineAudioProcessorEditor(VstEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      header(p.parameters(),{{},{},[this]{showPage(Page::presets);},[&p]{p.requestPanic();}}),
      rackPage(p), sequencePage(p), presets(*p.presetManager()), settings(p),
      pages{&rackPage,&sequencePage,&presets,&settings}
{
    addAndMakeVisible(header);addAndMakeVisible(navigation);for(auto*page:pages)addChildComponent(page);
    navigation.onPageChanged=[this](Page page){showPage(page);};setResizable(true,true);setResizeLimits(1040,680,1600,1000);setSize(1240,800);showPage(Page::rack);startTimerHz(20);
}
VstEngineAudioProcessorEditor::~VstEngineAudioProcessorEditor(){stopTimer();}
void VstEngineAudioProcessorEditor::paint(juce::Graphics&g){g.fillAll(vstengine::ui::colours::background);}
void VstEngineAudioProcessorEditor::resized(){auto a=getLocalBounds();header.setBounds(a.removeFromTop(58));navigation.setBounds(a.removeFromTop(44).reduced(10,4));for(auto*p:pages)p->setBounds(a.reduced(10,6));}
void VstEngineAudioProcessorEditor::showPage(Page page){const auto i=static_cast<std::size_t>(page);if(i>=pages.size())return;for(std::size_t n=0;n<pages.size();++n)pages[n]->setVisible(n==i);if(navigation.getCurrentPage()!=page)navigation.setCurrentPage(page);}
void VstEngineAudioProcessorEditor::showPageForTesting(Page page){showPage(page);}
Page VstEngineAudioProcessorEditor::currentPageForTesting()const noexcept{return navigation.getCurrentPage();}
float VstEngineAudioProcessorEditor::keyboardKeyWidthForTesting(Page)const noexcept{return rackPage.keyboardKeyWidth();}
int VstEngineAudioProcessorEditor::keyboardComponentWidthForTesting(Page)const noexcept{return rackPage.keyboardWidth();}
void VstEngineAudioProcessorEditor::timerCallback(){processor.publishSequenceForAudio();rackPage.refresh();sequencePage.setPlayHead(processor.getCurrentPlayHeadStep());sequencePage.refresh();header.setTransportActive(processor.getCurrentPlayHeadStep()>=0);if(auto*m=processor.presetManager())header.setPresetName(m->getCurrentPresetName());}

juce::AudioProcessorEditor* VstEngineAudioProcessor::createEditor(){return new VstEngineAudioProcessorEditor(*this);}
