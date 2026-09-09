#pragma once
#include "common/UiComponents.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace vstengine::ui {
class SettingsPanel final : public juce::Component {
public:
    struct Callbacks { std::function<juce::File()> createMidiFile; std::function<void()> panic; };
    SettingsPanel (juce::AudioProcessorValueTreeState&, juce::MidiKeyboardState&, Callbacks);
    void paint (juce::Graphics&) override; void resized() override;
private:
    class MidiDragButton final : public juce::TextButton {
    public: explicit MidiDragButton(std::function<juce::File()>); void mouseDown(const juce::MouseEvent&) override; void mouseDrag(const juce::MouseEvent&) override;
    private: std::function<juce::File()> createFile; bool started{};
    };
    juce::Label midiTitle,generatorTitle,systemTitle,bassChannel,status;
    juce::Label midiSourceLabel,generatorChannelLabel,kickChannelLabel,rootNoteLabel,seedLabel;
    juce::ComboBox midiMode;
    juce::Slider generatorChannel,kickChannel,rootNote,seed;
    juce::TextButton panicButton{"PANIC"}; MidiDragButton dragButton;
    juce::MidiKeyboardComponent keyboard;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<CA> modeAttachment;std::array<std::unique_ptr<SA>,4> attachments;
};
} // namespace vstengine::ui
