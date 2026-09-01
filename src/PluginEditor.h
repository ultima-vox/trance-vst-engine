#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MidiDragButton final : public juce::TextButton {
public:
    explicit MidiDragButton(VstEngineAudioProcessor& processorToUse)
        : processor(processorToUse)
    {
        setButtonText("DRAG MIDI TO CUBASE");
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStarted = false;
        juce::TextButton::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        juce::TextButton::mouseDrag(event);

        if (dragStarted || event.getDistanceFromDragStart() < 6)
            return;

        const auto file = processor.createGeneratedMidiFile();

        if (!file.existsAsFile())
            return;

        dragStarted = true;

        juce::StringArray files;
        files.add(file.getFullPathName());

        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            files, false, this);
    }

private:
    VstEngineAudioProcessor& processor;
    bool dragStarted {};
};

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
    MidiDragButton midiDragButton;

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
