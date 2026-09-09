#include "PluginEditor.h"
#include <ctime>

using Page = vstengine::ui::MainNavigation::Page;

VstEngineAudioProcessorEditor::VstEngineAudioProcessorEditor (VstEngineAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      header (p.parameters(), { [this] { selectAdjacentPreset (-1); },
                                [this] { selectAdjacentPreset (1); },
                                [this] { showPage (Page::presets); },
                                [&p] { p.requestPanic(); } }),
      bass (p.parameters()), kick (p.parameters()), sequenceCallbacks (p),
      sequence (p.sequence(), &sequenceCallbacks),
      match (p.parameters(), { [&p] { return p.analyzeKickBassMatch(); },
                               [&p] (const auto& adjustments) { p.applyMatchAdjustments (adjustments); } }),
      presets (*p.presetManager(), [this] {
          sequence.refreshFromModel();
          processor.publishSequenceForAudio();
      }),
      settings (p.parameters(), p.keyboardState(),
                { [&p] { return p.createGeneratedMidiFile(); }, [&p] { p.requestPanic(); } }),
      pages { &bass, &kick, &sequence, &match, &presets, &settings }
{
    addAndMakeVisible (header); addAndMakeVisible (navigation);
    for (auto* page : pages) addChildComponent (page);
    navigation.onPageChanged = [this] (Page page) { showPage (page); };
    setResizable (true, true); setResizeLimits (1040, 680, 1600, 1000); setSize (1180, 760);
    showPage (Page::bass); startTimerHz (30);
}

VstEngineAudioProcessorEditor::~VstEngineAudioProcessorEditor() { stopTimer(); }
void VstEngineAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (vstengine::ui::colours::background); }
void VstEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds(); header.setBounds (area.removeFromTop (58));
    navigation.setBounds (area.removeFromTop (44).reduced (10, 4));
    for (auto* page : pages) page->setBounds (area.reduced (10, 6));
}
void VstEngineAudioProcessorEditor::showPage (Page page)
{
    const auto index = static_cast<size_t> (page);
    if (index >= pages.size()) return;
    for (size_t i=0;i<pages.size();++i) pages[i]->setVisible (i==index);
    if (navigation.getCurrentPage()!=page) navigation.setCurrentPage(page);
}
void VstEngineAudioProcessorEditor::showPageForTesting (Page page) { showPage (page); }
Page VstEngineAudioProcessorEditor::currentPageForTesting() const noexcept { return navigation.getCurrentPage(); }
void VstEngineAudioProcessorEditor::timerCallback()
{
    processor.publishSequenceForAudio();
    const int step=processor.getCurrentPlayHeadStep(); sequence.setPlayHeadPosition(step);
    match.refreshCurrentValues();
    header.setTransportActive(step>=0); if(auto*m=processor.presetManager())header.setPresetName(m->getCurrentPresetName());
}
void VstEngineAudioProcessorEditor::selectAdjacentPreset (int delta)
{
    auto* manager=processor.presetManager();if(!manager)return;const auto entries=manager->getPresets();if(entries.isEmpty())return;
    int current=0;const auto name=manager->getCurrentPresetName();for(int i=0;i<entries.size();++i)if(entries.getReference(i).name==name){current=i;break;}
    const int next=juce::jlimit(0,entries.size()-1,current+delta);if(manager->loadPreset(entries.getReference(next))){sequence.refreshFromModel();processor.publishSequenceForAudio();}presets.refresh();
}

void VstEngineAudioProcessorEditor::SequencerCallbacks::onCopy(){processor.sequence().copyTo(clipboard);hasClipboard=true;}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onPaste(){if(hasClipboard)processor.sequence().paste(clipboard);}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onRotateLeft(){processor.sequence().rotateLeft();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onRotateRight(){processor.sequence().rotateRight();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onReverse(){processor.sequence().reverse();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onShiftLeft(){processor.sequence().shiftLeft();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onShiftRight(){processor.sequence().shiftRight();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onTransposeUp(){processor.sequence().transpose(1);}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onTransposeDown(){processor.sequence().transpose(-1);}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onOctaveUp(){processor.sequence().octaveUp();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onOctaveDown(){processor.sequence().octaveDown();}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onMutate(){processor.sequence().mutateSelected((int)std::time(nullptr));}
void VstEngineAudioProcessorEditor::SequencerCallbacks::onClear(){processor.sequence().clearSelected();}

juce::AudioProcessorEditor* VstEngineAudioProcessor::createEditor(){return new VstEngineAudioProcessorEditor(*this);}
