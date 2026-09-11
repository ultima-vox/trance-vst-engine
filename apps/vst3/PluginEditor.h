#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/GlobalHeader.h"
#include "ui/MainNavigation.h"
#include "ui/PresetBrowser.h"
#include "ui/StepSequencer.h"

class VstEngineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer {
public:
    explicit VstEngineAudioProcessorEditor(VstEngineAudioProcessor&);
    ~VstEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void showPageForTesting(vstengine::ui::MainNavigation::Page);
    [[nodiscard]] vstengine::ui::MainNavigation::Page
        currentPageForTesting() const noexcept;
    [[nodiscard]] float keyboardKeyWidthForTesting(
        vstengine::ui::MainNavigation::Page) const noexcept;
    [[nodiscard]] int keyboardComponentWidthForTesting(
        vstengine::ui::MainNavigation::Page) const noexcept;

private:
    class RackPage final : public juce::Component {
    public:
        explicit RackPage(VstEngineAudioProcessor&);
        void paint(juce::Graphics&) override;
        void resized() override;
        void refresh();
        float keyboardKeyWidth() const noexcept { return keyboard.getKeyWidth(); }
        int keyboardWidth() const noexcept { return keyboard.getWidth(); }
    private:
        void select(std::size_t);
        void bindSelectedSlot();
        void applyChannel(VstEngineAudioProcessor::ChannelConflictAction);
        VstEngineAudioProcessor& processor;
        std::array<juce::TextButton, vstengine::instrument::maxSlots> slots;
        juce::ComboBox instrument, midiIn;
        juce::TextButton swap { "SWAP" }, move { "MOVE" }, layerAction { "LAYER" };
        juce::ToggleButton layer { "Layer" }, enabled { "Enabled" },
            mute { "Mute" }, solo { "Solo" }, locked { "Lock" };
        juce::Slider keyLow, keyHigh, velocityLow, velocityHigh, transpose,
            level, pan;
        std::array<juce::Slider, vstengine::instrument::macrosPerSlot> macros;
        std::array<juce::Label, vstengine::instrument::macrosPerSlot> macroLabels;
        juce::Label title, routingTitle, macroTitle, status;
        juce::MidiKeyboardComponent keyboard;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>
            sliderAttachments;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>
            buttonAttachments;
        std::size_t selected {};
        int pendingChannel {};
    };

    struct SequenceCallbacks final : vstengine::ui::StepSequencer::Callbacks {
        explicit SequenceCallbacks(VstEngineAudioProcessor& p) : processor(p) {}
        void onCopy() override; void onPaste() override;
        void onRotateLeft() override; void onRotateRight() override;
        void onReverse() override; void onShiftLeft() override; void onShiftRight() override;
        void onTransposeUp() override; void onTransposeDown() override;
        void onOctaveUp() override; void onOctaveDown() override;
        void onMutate() override; void onClear() override;
        void onSequenceChanged() override;
        VstEngineAudioProcessor& processor;
        vstengine::sequence::Sequence clipboard;
        bool copied {};
        std::uint32_t mutationOrdinal {};
    };

    class SequencePage final : public juce::Component {
    public:
        explicit SequencePage(VstEngineAudioProcessor&);
        void paint(juce::Graphics&) override;
        void resized() override;
        void refresh();
        void setPlayHead(int);
    private:
        VstEngineAudioProcessor& processor;
        SequenceCallbacks callbacks;
        vstengine::ui::StepSequencer sequencer;
        juce::Label status;
    };

    class SettingsPage final : public juce::Component {
    public:
        explicit SettingsPage(VstEngineAudioProcessor&);
        void paint(juce::Graphics&) override;
        void resized() override;
    private:
        juce::Label title, description;
        juce::ComboBox midiMode;
        juce::Slider seed;
        juce::TextButton panic { "GLOBAL PANIC" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedAttachment;
    };

    void timerCallback() override;
    void showPage(vstengine::ui::MainNavigation::Page);
    VstEngineAudioProcessor& processor;
    vstengine::ui::GlobalHeader header;
    vstengine::ui::MainNavigation navigation;
    RackPage rackPage;
    SequencePage sequencePage;
    vstengine::ui::PresetBrowser presets;
    SettingsPage settings;
    std::array<juce::Component*, 4> pages;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessorEditor)
};
