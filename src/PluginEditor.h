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

    // Title & MIDI source
    juce::ComboBox midiModeBox;
    MidiDragButton midiDragButton;
    juce::MidiKeyboardComponent pianoKeyboard;

    // Generator settings
    juce::Slider midiChannelSlider;
    juce::Slider rootNoteSlider;

    // Pitch envelope
    juce::Slider pitchEnvAmountSlider;
    juce::Slider pitchEnvTimeSlider;
    juce::Slider pitchEnvCurveSlider;

    // ADSR
    juce::Slider ampAttackSlider;
    juce::Slider ampDecaySlider;
    juce::Slider ampSustainSlider;
    juce::Slider ampReleaseSlider;

    // Filter
    juce::Slider filterCutoffSlider;
    juce::Slider filterResonanceSlider;
    juce::Slider filterDriveSlider;
    juce::Slider keyTrackingSlider;

    // Output
    juce::Slider driveSlider;
    juce::Slider outputLevelSlider;

    // Labels
    juce::Label midiModeLabel;
    juce::Label midiChannelLabel;
    juce::Label rootNoteLabel;
    juce::Label pitchEnvAmountLabel;
    juce::Label pitchEnvTimeLabel;
    juce::Label pitchEnvCurveLabel;
    juce::Label ampAttackLabel;
    juce::Label ampDecayLabel;
    juce::Label ampSustainLabel;
    juce::Label ampReleaseLabel;
    juce::Label filterCutoffLabel;
    juce::Label filterResonanceLabel;
    juce::Label filterDriveLabel;
    juce::Label keyTrackingLabel;
    juce::Label driveLabel;
    juce::Label outputLevelLabel;
    juce::Label titleLabel;

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Attachments
    std::unique_ptr<ComboBoxAttachment> midiModeAttachment;
    std::unique_ptr<SliderAttachment> midiChannelAttachment;
    std::unique_ptr<SliderAttachment> rootNoteAttachment;
    std::unique_ptr<SliderAttachment> pitchEnvAmountAttachment;
    std::unique_ptr<SliderAttachment> pitchEnvTimeAttachment;
    std::unique_ptr<SliderAttachment> pitchEnvCurveAttachment;
    std::unique_ptr<SliderAttachment> ampAttackAttachment;
    std::unique_ptr<SliderAttachment> ampDecayAttachment;
    std::unique_ptr<SliderAttachment> ampSustainAttachment;
    std::unique_ptr<SliderAttachment> ampReleaseAttachment;
    std::unique_ptr<SliderAttachment> filterCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterResonanceAttachment;
    std::unique_ptr<SliderAttachment> filterDriveAttachment;
    std::unique_ptr<SliderAttachment> keyTrackingAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> outputLevelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        VstEngineAudioProcessorEditor)
};
