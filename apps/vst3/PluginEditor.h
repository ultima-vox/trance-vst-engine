#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/BassPanel.h"
#include "ui/GlobalHeader.h"
#include "ui/KickPanel.h"
#include "ui/MainNavigation.h"
#include "ui/MatchPanel.h"
#include "ui/PresetBrowser.h"
#include "ui/SettingsPanel.h"
#include "ui/StepSequencer.h"

class VstEngineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer {
public:
    explicit VstEngineAudioProcessorEditor (VstEngineAudioProcessor&);
    ~VstEngineAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPageForTesting (vstengine::ui::MainNavigation::Page);
    [[nodiscard]] vstengine::ui::MainNavigation::Page currentPageForTesting() const noexcept;

private:
    struct SequencerCallbacks final : vstengine::ui::StepSequencer::Callbacks {
        explicit SequencerCallbacks (VstEngineAudioProcessor& p) : processor (p) {}
        void onCopy() override; void onPaste() override;
        void onRotateLeft() override; void onRotateRight() override; void onReverse() override;
        void onShiftLeft() override; void onShiftRight() override;
        void onTransposeUp() override; void onTransposeDown() override;
        void onOctaveUp() override; void onOctaveDown() override;
        void onMutate() override; void onClear() override;
        void onSequenceChanged() override { processor.publishSequenceForAudio(); }
        VstEngineAudioProcessor& processor;
        vstengine::sequence::Sequence clipboard;
        bool hasClipboard {};
    };
    void timerCallback() override;
    void showPage (vstengine::ui::MainNavigation::Page);
    void selectAdjacentPreset (int delta);

    VstEngineAudioProcessor& processor;
    vstengine::ui::GlobalHeader header;
    vstengine::ui::MainNavigation navigation;
    vstengine::ui::BassPanel bass;
    vstengine::ui::KickPanel kick;
    SequencerCallbacks sequenceCallbacks;
    vstengine::ui::StepSequencer sequence;
    vstengine::ui::MatchPanel match;
    vstengine::ui::PresetBrowser presets;
    vstengine::ui::SettingsPanel settings;
    std::array<juce::Component*, 6> pages;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VstEngineAudioProcessorEditor)
};
