#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/StepSequencer.h"

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

class VstEngineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            public juce::Timer {
public:
    explicit VstEngineAudioProcessorEditor(VstEngineAudioProcessor&);
    ~VstEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VstEngineAudioProcessor& processor;

    // MIDI source + generator settings
    juce::ComboBox midiModeBox;
    MidiDragButton midiDragButton;
    juce::MidiKeyboardComponent pianoKeyboard;
    juce::Slider midiChannelSlider;
    juce::Slider rootNoteSlider;
    juce::Slider rngSeedSlider;

    // Resonant filter
    juce::Slider filterCutoffSlider;
    juce::Slider filterResonanceSlider;
    juce::Slider filterDriveSlider;
    juce::Slider keyTrackingSlider;

    // Amp envelope (per-parameter ADSR)
    juce::Slider ampAttackSlider;
    juce::Slider ampDecaySlider;
    juce::Slider ampSustainSlider;
    juce::Slider ampReleaseSlider;

    // Pitch envelope
    juce::Slider pitchEnvAmountSlider;
    juce::Slider pitchEnvTimeSlider;
    juce::Slider pitchEnvCurveSlider;

    // Output
    juce::Slider driveSlider;
    juce::Slider outputLevelSlider;

    // Kick engine (issue #11 PHASE 5)
    juce::Slider kickPitchStartSlider;
    juce::Slider kickPitchEndSlider;
    juce::Slider kickPitchDecaySlider;
    juce::Slider kickPitchCurveSlider;
    juce::Slider kickBodyDecaySlider;
    juce::Slider kickTailSlider;
    juce::Slider kickClickSlider;
    juce::Slider kickClickToneSlider;
    juce::Slider kickDriveSlider;
    juce::Slider kickClipSlider;
    juce::Slider kickTransientSlider;
    juce::Slider kickSubSlider;
    juce::Slider kickTuneSlider;
    juce::Slider kickPhaseSlider;
    juce::Slider kickOutputLevelSlider;
    juce::Slider kickMidiChannelSlider;

    // Labels
    juce::Label titleLabel;
    juce::Label presetLabel;
    juce::Label midiModeLabel;
    juce::Label midiChannelLabel;
    juce::Label rootNoteLabel;
    juce::Label rngSeedLabel;
    juce::Label filterGroupLabel;
    juce::Label ampGroupLabel;
    juce::Label pitchGroupLabel;
    juce::Label outputGroupLabel;
    juce::Label filterCutoffLabel;
    juce::Label filterResonanceLabel;
    juce::Label filterDriveLabel;
    juce::Label keyTrackingLabel;
    juce::Label ampAttackLabel;
    juce::Label ampDecayLabel;
    juce::Label ampSustainLabel;
    juce::Label ampReleaseLabel;
    juce::Label pitchEnvAmountLabel;
    juce::Label pitchEnvTimeLabel;
    juce::Label pitchEnvCurveLabel;
    juce::Label driveLabel;
    juce::Label outputLevelLabel;
    juce::Label kickGroupLabel;
    juce::Label kickPitchStartLabel;
    juce::Label kickPitchEndLabel;
    juce::Label kickPitchDecayLabel;
    juce::Label kickPitchCurveLabel;
    juce::Label kickBodyDecayLabel;
    juce::Label kickTailLabel;
    juce::Label kickClickLabel;
    juce::Label kickClickToneLabel;
    juce::Label kickDriveLabel;
    juce::Label kickClipLabel;
    juce::Label kickTransientLabel;
    juce::Label kickSubLabel;
    juce::Label kickTuneLabel;
    juce::Label kickPhaseLabel;
    juce::Label kickOutputLevelLabel;
    juce::Label kickMidiChannelLabel;

    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Preset UI
    juce::ComboBox presetBox;
    juce::TextEditor presetNameEditor;
    juce::TextButton presetSaveButton { "SAVE" };
    juce::TextButton presetSaveFullButton { "SAVE FULL" };
    juce::TextButton presetLoadButton { "LOAD" };
    juce::TextButton presetRenameButton { "RENAME" };
    juce::TextButton presetDeleteButton { "DELETE" };
    juce::TextButton presetRefreshButton { "REFRESH" };
    enum class PresetKind { factory, userSound, userFull };
    PresetKind presetKindForId (int itemId) const noexcept;
    void refreshPresetList();

    std::unique_ptr<ComboBoxAttachment> midiModeAttachment;
    std::unique_ptr<SliderAttachment> midiChannelAttachment;
    std::unique_ptr<SliderAttachment> rootNoteAttachment;
    std::unique_ptr<SliderAttachment> rngSeedAttachment;
    std::unique_ptr<SliderAttachment> filterCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterResonanceAttachment;
    std::unique_ptr<SliderAttachment> filterDriveAttachment;
    std::unique_ptr<SliderAttachment> keyTrackingAttachment;
    std::unique_ptr<SliderAttachment> ampAttackAttachment;
    std::unique_ptr<SliderAttachment> ampDecayAttachment;
    std::unique_ptr<SliderAttachment> ampSustainAttachment;
    std::unique_ptr<SliderAttachment> ampReleaseAttachment;
    std::unique_ptr<SliderAttachment> pitchEnvAmountAttachment;
    std::unique_ptr<SliderAttachment> pitchEnvTimeAttachment;
    std::unique_ptr<SliderAttachment> pitchEnvCurveAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> outputLevelAttachment;
    std::unique_ptr<SliderAttachment> kickPitchStartAttachment;
    std::unique_ptr<SliderAttachment> kickPitchEndAttachment;
    std::unique_ptr<SliderAttachment> kickPitchDecayAttachment;
    std::unique_ptr<SliderAttachment> kickPitchCurveAttachment;
    std::unique_ptr<SliderAttachment> kickBodyDecayAttachment;
    std::unique_ptr<SliderAttachment> kickTailAttachment;
    std::unique_ptr<SliderAttachment> kickClickAttachment;
    std::unique_ptr<SliderAttachment> kickClickToneAttachment;
    std::unique_ptr<SliderAttachment> kickDriveAttachment;
    std::unique_ptr<SliderAttachment> kickClipAttachment;
    std::unique_ptr<SliderAttachment> kickTransientAttachment;
    std::unique_ptr<SliderAttachment> kickSubAttachment;
    std::unique_ptr<SliderAttachment> kickTuneAttachment;
    std::unique_ptr<SliderAttachment> kickPhaseAttachment;
    std::unique_ptr<SliderAttachment> kickOutputLevelAttachment;
    std::unique_ptr<SliderAttachment> kickMidiChannelAttachment;

    // Sequencer callback implementation
    struct SequencerCallbacks
        : public vstengine::ui::StepSequencer::Callbacks {
        explicit SequencerCallbacks (VstEngineAudioProcessor& p)
            : processor (p) {}
        void onCopy() override;
        void onPaste() override;
        void onRotateLeft() override;
        void onRotateRight() override;
        void onReverse() override;
        void onShiftLeft() override;
        void onShiftRight() override;
        void onTransposeUp() override;
        void onTransposeDown() override;
        void onOctaveUp() override;
        void onOctaveDown() override;
        void onMutate() override;
        void onClear() override;

        VstEngineAudioProcessor& processor;
        vstengine::sequence::Sequence clipboard;
        bool hasClipboard { false };
    };

    std::unique_ptr<SequencerCallbacks> seqCallbacks;
    std::unique_ptr<vstengine::ui::StepSequencer> sequencer;

    void timerCallback() override
    { sequencer->setPlayHeadPosition(processor.getCurrentPlayHeadStep()); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        VstEngineAudioProcessorEditor)
};