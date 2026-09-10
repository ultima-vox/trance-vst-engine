#include "StepSequencer.h"
#include "common/UiComponents.h"
#include <cmath>

namespace vstengine::ui {
StepSequencer::StepSequencer (vstengine::sequence::Sequence& seq, Callbacks* cb)
    : sequence (seq), callbacks (cb)
{
    lengthBox.addItemList ({ "16 steps", "32 steps", "64 steps" }, 1);
    lengthBox.setSelectedId (sequence.size() <= 16 ? 1 : sequence.size() <= 32 ? 2 : 3,
                             juce::dontSendNotification);
    lengthBox.onChange = [this] { static constexpr int n[] { 16, 32, 64 };
        sequence.setLength (n[juce::jlimit (0, 2, lengthBox.getSelectedId() - 1)]); if(callbacks)callbacks->onSequenceChanged(); repaint(); };
    timingBox.addItemList ({ "1/8", "1/16", "1/32", "Triplet" }, 1);
    const auto tm = sequence.getTimingMode();
    timingBox.setSelectedId (tm == vstengine::sequence::TimingMode::eighth ? 1
        : tm == vstengine::sequence::TimingMode::sixteenth ? 2
        : tm == vstengine::sequence::TimingMode::thirtySecond ? 3 : 4, juce::dontSendNotification);
    timingBox.onChange = [this] { static constexpr vstengine::sequence::TimingMode modes[] {
        vstengine::sequence::TimingMode::eighth, vstengine::sequence::TimingMode::sixteenth,
        vstengine::sequence::TimingMode::thirtySecond, vstengine::sequence::TimingMode::triplet };
        sequence.setTimingMode (modes[juce::jlimit (0, 3, timingBox.getSelectedId() - 1)]); if(callbacks)callbacks->onSequenceChanged(); repaint(); };
    addAndMakeVisible (lengthBox); addAndMakeVisible (timingBox);
    static constexpr const char* lanes[] { "NOTE", "GATE", "VELOCITY", "ACCENT", "PROBABILITY", "RATCHET", "SLIDE" };
    for (size_t i = 0; i < laneButtons.size(); ++i) { auto& b = laneButtons[i]; b.setButtonText (lanes[i]);
        b.onClick = [this, i] { setLane (static_cast<Lane> (i)); }; styleButton (b); addAndMakeVisible (b); }
    static constexpr const char* actions[] { "Copy", "Paste", "Rotate Left", "Rotate Right", "Reverse", "Shift Left",
        "Shift Right", "Transpose +", "Transpose -", "Octave +", "Octave -", "Mutate", "Clear" };
    for (size_t i = 0; i < actionButtons.size(); ++i) { auto& b = actionButtons[i]; b.setButtonText (actions[i]);
        b.onClick = [this, i] { fireAction (static_cast<int> (i)); }; styleButton (b, i == 11); addAndMakeVisible (b); }
    setLane (Lane::note);
}
void StepSequencer::setLane (Lane value) { if (value < Lane::note || value >= Lane::count) return; lane = value;
    for (size_t i = 0; i < laneButtons.size(); ++i) laneButtons[i].setToggleState (i == static_cast<size_t> (lane), juce::dontSendNotification); repaint(); }
void StepSequencer::setSlideEnabled (const bool enabled)
{
    slideEnabled = enabled;
    laneButtons[static_cast<size_t> (Lane::slide)].setEnabled (enabled);
    if (! enabled && lane == Lane::slide)
        setLane (Lane::note);
}
void StepSequencer::refreshFromModel()
{
    lengthBox.setSelectedId(sequence.size() <= 16 ? 1 : sequence.size() <= 32 ? 2 : 3,
                            juce::dontSendNotification);
    const auto mode = sequence.getTimingMode();
    timingBox.setSelectedId(mode == vstengine::sequence::TimingMode::eighth ? 1
        : mode == vstengine::sequence::TimingMode::sixteenth ? 2
        : mode == vstengine::sequence::TimingMode::thirtySecond ? 3 : 4,
        juce::dontSendNotification);
    repaint();
}
void StepSequencer::setPlayHeadPosition (int step) { const int next = juce::jlimit (-1, sequence.size() - 1, step);
    if (next != playHeadStep) { playHeadStep = next; repaint (gridBounds()); } }
juce::Rectangle<int> StepSequencer::gridBounds() const { return getLocalBounds().withTrimmedTop (gridTop).reduced (8).withTrimmedLeft (gutterWidth); }
void StepSequencer::paint (juce::Graphics& g)
{
    g.fillAll (colours::background); const auto grid = gridBounds(); const int count = sequence.size();
    if (count <= 0 || grid.isEmpty()) return; const float column = static_cast<float> (grid.getWidth()) / count;
    static constexpr const char* names[] { "NOTE", "GATE", "VELOCITY", "ACCENT", "PROB.", "RATCHET", "SLIDE" };
    g.setColour (colours::mutedText); g.setFont (12.0f); g.drawText (names[static_cast<size_t> (lane)], 8, grid.getY(), gutterWidth, 22, juce::Justification::centredLeft);
    for (int i = 0; i < count; ++i) { const int x1 = grid.getX() + juce::roundToInt (column * i);
        const int x2 = grid.getX() + juce::roundToInt (column * (i + 1)); auto cell = juce::Rectangle<int> (x1, grid.getY(), juce::jmax (1, x2-x1), grid.getHeight());
        g.setColour (i % 4 == 0 ? colours::panelRaised : colours::panel); g.fillRect (cell.reduced (1, 0)); const auto& s = sequence[i]; g.setColour (colours::primary);
        if (lane == Lane::note) { const float norm = juce::jmap (static_cast<float> (s.noteOffset), -24.0f, 24.0f, 1.0f, 0.0f);
            const int y = grid.getY() + juce::roundToInt (norm * (grid.getHeight()-8)); g.fillRoundedRectangle (cell.getX()+2.0f, (float)y, juce::jmax (2.0f, cell.getWidth()-4.0f), 7.0f, 2.0f); }
        else if (lane == Lane::gate || lane == Lane::accent) { const bool on = lane == Lane::gate ? s.gate : s.accent; g.setColour (on ? colours::status : colours::border); g.fillRoundedRectangle (cell.reduced (3).toFloat(), 3.0f); }
        else { float v = lane == Lane::velocity ? s.velocity : lane == Lane::probability ? s.probability : lane == Lane::ratchet ? s.ratchetCount / 8.0f : s.slideDuration / 8.0f;
            v = juce::jlimit (0.0f,1.0f,v); auto bar=cell.reduced(3); bar.removeFromTop(juce::roundToInt((1.0f-v)*bar.getHeight())); g.fillRoundedRectangle(bar.toFloat(),2.0f);
            if (lane == Lane::ratchet && cell.getWidth() >= 22) g.drawText(juce::String(s.ratchetCount),cell,juce::Justification::centred); }
        g.setColour(colours::border); g.drawRect(cell, i%4==0?2:1); }
    if (playHeadStep >= 0 && playHeadStep < count) { const int x=grid.getX()+juce::roundToInt(column*playHeadStep); g.setColour(colours::status); g.fillRect(x,grid.getY(),3,grid.getHeight()); }
}
void StepSequencer::resized()
{
    auto top=getLocalBounds().reduced(8).removeFromTop(32); lengthBox.setBounds(top.removeFromLeft(112).reduced(2)); timingBox.setBounds(top.removeFromLeft(100).reduced(2));
    juce::FlexBox lanes; lanes.flexDirection=juce::FlexBox::Direction::row; for(auto& b:laneButtons) lanes.items.add(juce::FlexItem(b).withFlex(1).withMargin(2)); lanes.performLayout(top);
    auto actions=getLocalBounds().reduced(8).withTrimmedTop(38).removeFromTop(68); constexpr int half=7;
    for(int row=0;row<2;++row){auto rowArea=actions.removeFromTop(32); juce::FlexBox flex; flex.flexDirection=juce::FlexBox::Direction::row;
        for(int i=row*half;i<juce::jmin((int)actionButtons.size(),(row+1)*half);++i) flex.items.add(juce::FlexItem(actionButtons[(size_t)i]).withFlex(1).withMargin(2)); flex.performLayout(rowArea); actions.removeFromTop(2);}
}
int StepSequencer::stepAt (juce::Point<int> p) const { const auto grid=gridBounds(); if(!grid.contains(p)) return -1;
    return juce::jlimit(0,sequence.size()-1,(p.x-grid.getX())*sequence.size()/juce::jmax(1,grid.getWidth())); }
void StepSequencer::editAt (juce::Point<int> p, bool initial)
{
    if (! slideEnabled && lane == Lane::slide) return;
    const int i=stepAt(p); if(i<0)return; auto& s=sequence[i]; const auto grid=gridBounds(); const float v=juce::jlimit(0.0f,1.0f,1.0f-(float)(p.y-grid.getY())/juce::jmax(1,grid.getHeight()));
    if(lane==Lane::note)s.noteOffset=juce::jlimit(-24,24,juce::roundToInt(v*48-24)); else if(lane==Lane::gate&&(initial||i!=selectedStep))s.gate=!s.gate;
    else if(lane==Lane::velocity)s.velocity=v; else if(lane==Lane::accent&&(initial||i!=selectedStep))s.accent=!s.accent; else if(lane==Lane::probability)s.probability=v;
    else if(lane==Lane::ratchet)s.ratchetCount=juce::jlimit(1,8,1+juce::roundToInt(v*7)); else if(lane==Lane::slide)s.slideDuration=juce::jlimit(0.0f,8.0f,std::round(v*8));
    selectedStep=i; sequence.setSelectedRange(i,i); if(callbacks)callbacks->onSequenceChanged(); repaint(grid);
}
void StepSequencer::mouseDown(const juce::MouseEvent& e){editAt(e.getPosition(),true);} void StepSequencer::mouseDrag(const juce::MouseEvent& e){editAt(e.getPosition(),false);}
void StepSequencer::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w){const int i=stepAt(e.getPosition());if(i<0)return;auto& s=sequence[i];const int d=w.deltaY>0?1:-1;
    if (!slideEnabled && lane == Lane::slide) return;
    if(lane==Lane::note)s.noteOffset=juce::jlimit(-24,24,s.noteOffset+d);else if(lane==Lane::velocity)s.velocity=juce::jlimit(0.0f,1.0f,s.velocity+d*.05f);else if(lane==Lane::probability)s.probability=juce::jlimit(0.0f,1.0f,s.probability+d*.05f);else if(lane==Lane::ratchet)s.ratchetCount=juce::jlimit(1,8,s.ratchetCount+d);else if(lane==Lane::slide)s.slideDuration=juce::jlimit(0.0f,8.0f,s.slideDuration+d);if(callbacks)callbacks->onSequenceChanged();repaint(gridBounds());}
void StepSequencer::fireAction(int a){if(!callbacks)return;switch(a){case 0:callbacks->onCopy();break;case 1:callbacks->onPaste();break;case 2:callbacks->onRotateLeft();break;case 3:callbacks->onRotateRight();break;case 4:callbacks->onReverse();break;case 5:callbacks->onShiftLeft();break;case 6:callbacks->onShiftRight();break;case 7:callbacks->onTransposeUp();break;case 8:callbacks->onTransposeDown();break;case 9:callbacks->onOctaveUp();break;case 10:callbacks->onOctaveDown();break;case 11:callbacks->onMutate();break;case 12:callbacks->onClear();break;default:break;}callbacks->onSequenceChanged();repaint();}
} // namespace vstengine::ui
