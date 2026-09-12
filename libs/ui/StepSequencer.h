#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "sequence/Sequence.h"
#include <array>

namespace vstengine::ui {
class StepSequencer final : public juce::Component {
public:
    struct Callbacks {
        virtual ~Callbacks() = default;
        virtual void onCopy() = 0; virtual void onPaste() = 0;
        virtual void onRotateLeft() = 0; virtual void onRotateRight() = 0;
        virtual void onReverse() = 0; virtual void onShiftLeft() = 0;
        virtual void onShiftRight() = 0; virtual void onTransposeUp() = 0;
        virtual void onTransposeDown() = 0; virtual void onOctaveUp() = 0;
        virtual void onOctaveDown() = 0; virtual void onMutate() = 0;
        virtual void onClear() = 0;
        virtual void onSequenceChanged() {}
    };
    enum class Lane { note, gate, velocity, accent, probability, ratchet, slide, count };
    explicit StepSequencer (vstengine::sequence::Sequence&, Callbacks* = nullptr);
    void setSequence(vstengine::sequence::Sequence&) noexcept;
    void paint (juce::Graphics&) override; void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setPlayHeadPosition (int);
    void setLane (Lane);
    void setSlideEnabled (bool enabled);
    void refreshFromModel();
    [[nodiscard]] Lane getLane() const noexcept { return lane; }
private:
    vstengine::sequence::Sequence* sequence {};
    Callbacks* callbacks {};
    juce::ComboBox lengthBox, timingBox;
    std::array<juce::TextButton, static_cast<size_t> (Lane::count)> laneButtons;
    std::array<juce::TextButton, 13> actionButtons;
    Lane lane { Lane::note };
    bool slideEnabled { true };
    int playHeadStep { -1 }, selectedStep { -1 };
    static constexpr int gridTop = 118, gutterWidth = 76;
    [[nodiscard]] juce::Rectangle<int> gridBounds() const;
    [[nodiscard]] int stepAt (juce::Point<int>) const;
    void editAt (juce::Point<int>, bool); void fireAction (int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencer)
};
} // namespace vstengine::ui
