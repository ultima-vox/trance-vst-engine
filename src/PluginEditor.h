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
    juce::Slider midiChannelSlider;
    juce::Slider rootNoteSlider;
    juce::ComboBox midiModeBox;

    juce::Label driveLabel;
    juce::Label releaseLabel;
    juce::Label midiModeLabel;
    juce::Label midiChannelLabel;
    juce::Label rootNoteLabel;
    juce::Label titleLabel;

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> midiChannelAttachment;
    std::unique_ptr<SliderAttachment> rootNoteAttachment;
    std::unique_ptr<ComboBoxAttachment> midiModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        VstEngineAudioProcessorEditor)
};
