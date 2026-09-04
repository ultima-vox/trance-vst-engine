#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "sequence/Sequence.h"

namespace vstengine::ui {

class StepSequencer final : public juce::Component {
public:
    struct Callbacks {
        virtual ~Callbacks() = default;
        virtual void onCopy() = 0;
        virtual void onPaste() = 0;
        virtual void onRotateLeft() = 0;
        virtual void onRotateRight() = 0;
        virtual void onReverse() = 0;
        virtual void onShiftLeft() = 0;
        virtual void onShiftRight() = 0;
        virtual void onTransposeUp() = 0;
        virtual void onTransposeDown() = 0;
        virtual void onOctaveUp() = 0;
        virtual void onOctaveDown() = 0;
        virtual void onMutate() = 0;
        virtual void onClear() = 0;
    };

    explicit StepSequencer(vstengine::sequence::Sequence& seq, Callbacks* cb = nullptr);
    ~StepSequencer() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Set the play head position (0 to size()-1)
    void setPlayHeadPosition(int step);

private:
    vstengine::sequence::Sequence& sequence;
    Callbacks* callbacks { nullptr };

    // Layout
    int columnWidth { 32 };
    int rowHeight { 28 };
    int headerHeight { 24 };
    int toolbarHeight { 36 };

    // Row indices
    static constexpr int rowPitch = 0;
    static constexpr int rowGate = 1;
    static constexpr int rowVelocity = 2;
    static constexpr int rowAccent = 3;
    static constexpr int rowProbability = 4;
    static constexpr int rowRatchet = 5;
    static constexpr int rowSlide = 6;
    static constexpr int numDataRows = 7;

    // Toolbar actions
    enum class ToolbarAction : int {
        copy = 0,
        paste,
        rotateLeft,
        rotateRight,
        reverse,
        shiftLeft,
        shiftRight,
        transposeUp,
        transposeDown,
        octaveUp,
        octaveDown,
        mutate,
        clear,
        numActions
    };

    // Selection
    int selectedStep { -1 };
    int selectedRow { -1 };
    bool isDragging { false };
    int dragStartStep { -1 };

    // Play head
    int playHeadStep { -1 };

    // Number of visible steps (16, 32, or 64)
    int visibleSteps { 16 };

    // Toolbar buttons
    std::unique_ptr<juce::TextButton> btnCopy;
    std::unique_ptr<juce::TextButton> btnPaste;
    std::unique_ptr<juce::TextButton> btnRotL;
    std::unique_ptr<juce::TextButton> btnRotR;
    std::unique_ptr<juce::TextButton> btnReverse;
    std::unique_ptr<juce::TextButton> btnShiftL;
    std::unique_ptr<juce::TextButton> btnShiftR;
    std::unique_ptr<juce::TextButton> btnTransposeUp;
    std::unique_ptr<juce::TextButton> btnTransposeDown;
    std::unique_ptr<juce::TextButton> btnOctaveUp;
    std::unique_ptr<juce::TextButton> btnOctaveDown;
    std::unique_ptr<juce::TextButton> btnMutate;
    std::unique_ptr<juce::TextButton> btnClear;

    void redraw();
    void setColumnWidth(int w);
    int getVisibleSteps() const noexcept;
    void handleRowAction(int step, int row, bool isMouseDown);
    void adjustWheelValue(int step, int row, bool forward);
    void fireToolbarAction(ToolbarAction action);

    // Drawing helpers
    void drawGrid(juce::Graphics& g);
    void drawHeader(juce::Graphics& g);
    void drawPitchRow(juce::Graphics& g);
    void drawGateRow(juce::Graphics& g);
    void drawVelocityRow(juce::Graphics& g);
    void drawAccentRow(juce::Graphics& g);
    void drawProbabilityRow(juce::Graphics& g);
    void drawRatchetRow(juce::Graphics& g);
    void drawSlideRow(juce::Graphics& g);
    void drawPlayHead(juce::Graphics& g);

    // Hit testing
    int hitTestStep(const juce::MouseEvent& event) const noexcept;
    int hitTestRow(const juce::MouseEvent& event) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencer)
};

} // namespace vstengine::ui
