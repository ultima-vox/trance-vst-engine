#include "BassPanel.h"

namespace vstengine::ui {

BassPanel::BassPanel (juce::AudioProcessorValueTreeState& state)
{
    filter.addKnob (state, "filterCutoff", "Cutoff", "Resonant filter cutoff");
    filter.addKnob (state, "filterResonance", "Resonance");
    filter.addKnob (state, "filterDrive", "Filter Drive");
    filter.addKnob (state, "keyTracking", "Key Track");
    amp.addKnob (state, "ampAttack", "Attack");
    amp.addKnob (state, "ampDecay", "Decay");
    amp.addKnob (state, "ampSustain", "Sustain");
    amp.addKnob (state, "release", "Release");
    pitch.addKnob (state, "pitchEnvAmount", "Env Amount");
    pitch.addKnob (state, "pitchEnvTime", "Env Time");
    pitch.addKnob (state, "pitchEnvCurve", "Curve");
    output.addKnob (state, "drive", "Drive");
    output.addKnob (state, "outputLevel", "Output");
    for (auto* section : { &filter, &amp, &pitch, &output })
        addAndMakeVisible (section);
}

void BassPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto top = area.removeFromTop ((area.getHeight() - 10) * 3 / 5);
    area.removeFromTop (10);
    auto bottom = area;
    auto layoutRow = [] (juce::Rectangle<int> bounds, juce::Component& left,
                         juce::Component& right) {
        juce::FlexBox row;
        row.flexDirection = juce::FlexBox::Direction::row;
        row.items.add (juce::FlexItem (left).withFlex (1.0f).withMargin (4.0f));
        row.items.add (juce::FlexItem (right).withFlex (1.0f).withMargin (4.0f));
        row.performLayout (bounds);
    };
    layoutRow (top, filter, amp);
    layoutRow (bottom, pitch, output);
}

} // namespace vstengine::ui
