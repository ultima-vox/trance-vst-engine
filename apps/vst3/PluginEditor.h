#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/BassPanel.h"
#include "ui/GlobalHeader.h"
#include "ui/KickPanel.h"
#include "ui/MainNavigation.h"
#include "ui/MatchPanel.h"
#include "ui/PresetBrowser.h"
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
    class MidiDragButton final : public juce::TextButton {
    public:
        MidiDragButton (juce::String text, std::function<juce::File()>);
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
    private:
        std::function<juce::File()> createFile;
        bool started {};
    };

    class InstrumentPage final : public juce::Component {
    public:
        InstrumentPage (juce::AudioProcessorValueTreeState&,
                        juce::MidiKeyboardState&,
                        vstengine::PresetManager&,
                        vstengine::PresetManager::SoundEngine,
                        juce::String partName,
                        const char* channelParameter,
                        const char* muteParameter,
                        const char* soloParameter,
                        const char* lockParameter,
                        const char* levelParameter,
                        const char* panParameter,
                        std::unique_ptr<juce::Component> enginePanel,
                        std::function<juce::File()> createMidiFile,
                        std::function<void()> onStateChanged);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refreshPresets();
    private:
        void loadSelectedPreset();
        void selectAdjacentPreset (int delta);
        void savePreset (bool saveAs);

        vstengine::PresetManager& presetManager;
        vstengine::PresetManager::SoundEngine engine;
        juce::Array<vstengine::PresetManager::PresetEntry> availablePresets;
        std::function<void()> stateChanged;
        std::unique_ptr<juce::Component> panel;
        juce::Label title, presetLabel, channelLabel, levelLabel, panLabel,
                    keyboardLabel, status;
        juce::ComboBox preset;
        juce::TextButton previous { "<" }, next { ">" }, save { "Save" },
                         saveAs { "Save As" }, mute { "M" }, solo { "S" },
                         lock { "Lock" };
        juce::Slider channel, level, pan;
        juce::MidiKeyboardComponent keyboard;
        MidiDragButton dragButton;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
        std::array<std::unique_ptr<SliderAttachment>, 3> sliderAttachments;
        std::array<std::unique_ptr<ButtonAttachment>, 3> buttonAttachments;
    };

    struct SequencerCallbacks final : vstengine::ui::StepSequencer::Callbacks {
        SequencerCallbacks (VstEngineAudioProcessor&, int partIndex);
        void onCopy() override; void onPaste() override;
        void onRotateLeft() override; void onRotateRight() override; void onReverse() override;
        void onShiftLeft() override; void onShiftRight() override;
        void onTransposeUp() override; void onTransposeDown() override;
        void onOctaveUp() override; void onOctaveDown() override;
        void onMutate() override; void onClear() override;
        void onSequenceChanged() override;
        [[nodiscard]] vstengine::sequence::Sequence& model() const;
        [[nodiscard]] bool editable() const;
        VstEngineAudioProcessor& processor;
        int partIndex;
        vstengine::sequence::Sequence clipboard;
        bool hasClipboard {};
    };

    class SequencerPage final : public juce::Component {
    public:
        explicit SequencerPage (VstEngineAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void setPlayHeadPositions (int bassStep, int kickStep);
        void setLockedParts (bool bassLocked, bool kickLocked);
        void refreshFromModels();
    private:
        void selectPart (int partIndex);
        juce::Label title { {}, "SEQ PART" };
        juce::TextButton bassButton { "BASS" }, kickButton { "KICK" };
        SequencerCallbacks bassCallbacks, kickCallbacks;
        vstengine::ui::StepSequencer bassSequence, kickSequence;
        int selectedPart {};
    };

    class SettingsPage final : public juce::Component {
    public:
        SettingsPage (juce::AudioProcessorValueTreeState&, std::function<void()> panic);
        void paint (juce::Graphics&) override;
        void resized() override;
    private:
        juce::Label midiTitle { {}, "GLOBAL MIDI" }, midiSourceLabel { {}, "MIDI Source" },
                    generatorTitle { {}, "GENERATOR" }, seedLabel { {}, "Global Seed" },
                    syncLabel { {}, "Host Sync" }, syncValue { {}, "Cubase Transport" },
                    systemTitle { {}, "SYSTEM" }, status { {}, "Panic resets all Parts and pending MIDI." };
        juce::ComboBox midiMode;
        juce::Slider seed;
        juce::TextButton panicButton { "PANIC ALL PARTS" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedAttachment;
    };

    void timerCallback() override;
    void showPage (vstengine::ui::MainNavigation::Page);
    void selectAdjacentPreset (int delta);
    void refreshPartUi();

    VstEngineAudioProcessor& processor;
    vstengine::ui::GlobalHeader header;
    vstengine::ui::MainNavigation navigation;
    InstrumentPage bass;
    InstrumentPage kick;
    SequencerPage sequence;
    vstengine::ui::MatchPanel match;
    vstengine::ui::PresetBrowser presets;
    SettingsPage settings;
    std::array<juce::Component*, 6> pages;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VstEngineAudioProcessorEditor)
};
