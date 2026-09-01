#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VstEngineAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit VstEngineAudioProcessorEditor(VstEngineAudioProcessor&);
    ~VstEngineAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VstEngineAudioProcessor& processor;

    juce::Slider driveSlider;
    juce::Slider releaseSlider;
    juce::Label driveLabel;
    juce::Label releaseLabel;
    juce::Label titleLabel;

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        VstEngineAudioProcessorEditor)
};
