#include "StepSequencer.h"

namespace vstengine::gui {

namespace {

// Color scheme (dark theme)
const juce::Colour bgColour = juce::Colour::fromRGB (11, 15, 26);
const juce::Colour gridColour = juce::Colour::fromRGB (45, 53, 70);
const juce::Colour playHeadColour = juce::Colour::fromRGB (185, 164, 105);
const juce::Colour pitchColour = juce::Colour::fromRGB (100, 140, 200);
const juce::Colour gateOnColour = juce::Colour::fromRGB (100, 200, 140);
const juce::Colour gateOffColour = juce::Colour::fromRGB (30, 35, 50);
const juce::Colour velocityColour = juce::Colour::fromRGB (140, 100, 200);
const juce::Colour accentColour = juce::Colour::fromRGB (200, 160, 60);
const juce::Colour ratchetColour = juce::Colour::fromRGB (70, 180, 180);
const juce::Colour slideColour = juce::Colour::fromRGB (220, 130, 80);
const juce::Colour headerTextColour = juce::Colour::fromRGB (180, 185, 200);
const juce::Colour downbeatTextColour = juce::Colour::fromRGB (220, 210, 170);
const juce::Colour selectedCellOverlay = juce::Colour::fromRGBA (255, 255, 255, 30);
const juce::Colour headerBgColour = juce::Colour::fromRGB (16, 20, 35);

juce::Rectangle<int> getStepRect (int stepIndex, int rowIndex,
                                   int colW, int rowH, int headerH, int toolbarH)
{
    return { stepIndex * colW,
             toolbarH + headerH + rowIndex * rowH,
             colW,
             rowH };
}

} // anonymous namespace

StepSequencer::StepSequencer (vstengine::generator::Sequence& seq, Callbacks* cb)
    : sequence (seq), callbacks (cb)
{
    setWantsKeyboardFocus (true);
    visibleSteps = getVisibleSteps();
    setColumnWidth (columnWidth);

    // Toolbar buttons
    auto addBtn = [this] (auto& btn, const juce::String& text, int x, int w) {
        btn = std::make_unique<juce::TextButton> (text);
        btn->setBounds (x, 4, w, toolbarHeight - 8);
        btn->setColour (juce::TextButton::buttonColourId, gridColour);
        btn->setColour (juce::TextButton::buttonColourId, gridColour.withAlpha (0.7f));
        addAndMakeVisible (*btn);
    };

    int x = 4;
    const int bw = 48;
    addBtn (btnCopy, "COPY", x, bw); x += bw + 2;
    addBtn (btnPaste, "PASTE", x, bw); x += bw + 2;
    addBtn (btnRotL, "ROT L", x, bw); x += bw + 2;
    addBtn (btnRotR, "ROT R", x, bw); x += bw + 2;
    addBtn (btnReverse, "REV", x, bw); x += bw + 2;
    addBtn (btnShiftL, "SH L", x, bw); x += bw + 2;
    addBtn (btnShiftR, "SH R", x, bw); x += bw + 2;
    addBtn (btnTransposeUp, "+1 sem", x, bw); x += bw + 2;
    addBtn (btnTransposeDown, "-1 sem", x, bw); x += bw + 2;
    addBtn (btnOctaveUp, "+8ve", x, bw); x += bw + 2;
    addBtn (btnOctaveDown, "-8ve", x, bw); x += bw + 2;
    addBtn (btnMutate, "MUTATE", x, bw); x += bw + 2;
    addBtn (btnClear, "CLEAR", x, bw);

    // Button callbacks
    btnCopy->onClick = [this] { fireToolbarAction (ToolbarAction::copy); };
    btnPaste->onClick = [this] { fireToolbarAction (ToolbarAction::paste); };
    btnRotL->onClick = [this] { fireToolbarAction (ToolbarAction::rotateLeft); };
    btnRotR->onClick = [this] { fireToolbarAction (ToolbarAction::rotateRight); };
    btnReverse->onClick = [this] { fireToolbarAction (ToolbarAction::reverse); };
    btnShiftL->onClick = [this] { fireToolbarAction (ToolbarAction::shiftLeft); };
    btnShiftR->onClick = [this] { fireToolbarAction (ToolbarAction::shiftRight); };
    btnTransposeUp->onClick = [this] { fireToolbarAction (ToolbarAction::transposeUp); };
    btnTransposeDown->onClick = [this] { fireToolbarAction (ToolbarAction::transposeDown); };
    btnOctaveUp->onClick = [this] { fireToolbarAction (ToolbarAction::octaveUp); };
    btnOctaveDown->onClick = [this] { fireToolbarAction (ToolbarAction::octaveDown); };
    btnMutate->onClick = [this] { fireToolbarAction (ToolbarAction::mutate); };
    btnClear->onClick = [this] { fireToolbarAction (ToolbarAction::clear); };
}

void StepSequencer::redraw()
{
    visibleSteps = getVisibleSteps();
    setColumnWidth (columnWidth);
    repaint();
}

int StepSequencer::getVisibleSteps() const noexcept
{
    const auto totalSteps = sequence.size();
    if (totalSteps <= 16) return 16;
    if (totalSteps <= 32) return 32;
    return totalSteps;
}

void StepSequencer::setColumnWidth (int w)
{
    columnWidth = juce::jlimit (24, 48, w);
    const int w2 = columnWidth * visibleSteps;
    const int h = toolbarHeight + headerHeight + numDataRows * rowHeight;
    if (getWidth() != w2 || getHeight() != h)
        setSize (w2, h);
}

void StepSequencer::setPlayHeadPosition (int step)
{
    playHeadStep = juce::jlimit (-1, sequence.size() - 1, step);
    repaint();
}

void StepSequencer::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    drawGrid (g);
    drawHeader (g);
    drawPitchRow (g);
    drawGateRow (g);
    drawVelocityRow (g);
    drawAccentRow (g);
    drawProbabilityRow (g);
    drawRatchetRow (g);
    drawSlideRow (g);
    drawPlayHead (g);
}

void StepSequencer::drawGrid (juce::Graphics& g)
{
    g.setColour (gridColour);

    const auto startY = toolbarHeight + headerHeight;

    // Horizontal lines
    for (int r = 0; r <= numDataRows; ++r) {
        const float y = static_cast<float> (startY + r * rowHeight);
        g.drawLine (0.5f, y,
                    static_cast<float> (columnWidth * visibleSteps), y, 1.0f);
    }

    // Vertical lines
    for (int c = 0; c <= visibleSteps; ++c) {
        const float x = static_cast<float> (c * columnWidth);
        g.drawLine (x, static_cast<float> (startY),
                    x, static_cast<float> (startY + numDataRows * rowHeight),
                    1.0f);
    }
}

void StepSequencer::drawHeader (juce::Graphics& g)
{
    const auto headerY = toolbarHeight;
    g.setColour (headerBgColour);
    g.fillRect (juce::Rectangle<int> (0, headerY,
                                      columnWidth * visibleSteps, headerHeight));

    g.setColour (headerTextColour);
    g.setFont (11.0f);

    for (int i = 0; i < visibleSteps; ++i) {
        const bool isDownbeat = (i % 4 == 0);
        g.setColour (isDownbeat ? downbeatTextColour : headerTextColour);

        const auto r = getStepRect (i, 0, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const juce::String num (juce::String (i + 1));
        g.drawText (num, r.withTrimmedLeft (4).withTrimmedRight (4),
                    juce::Justification::centred);
    }
}

void StepSequencer::drawPitchRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowPitch, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        // Background cell
        g.setColour (gateOffColour);
        g.fillRect (stepRect);

        // Pitch bar - height proportional to noteOffset
        if (step.noteOffset != 0) {
            const int maxOffset = 24; // two octaves
            const float ratio = static_cast<float> (step.noteOffset) / maxOffset;
            const int barH = juce::jlimit (2, rowHeight - 4,
                                           static_cast<int> ((rowHeight - 4) * (ratio + 0.5f)));
            const int barX = stepRect.getX() + (stepRect.getWidth() - columnWidth / 2) / 2;
            const int barY = stepRect.getCentreY() - barH / 2;
            g.setColour (pitchColour);
            g.fillRect (barX, barY, columnWidth / 2, barH);
        }

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowPitch) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawGateRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowGate, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        if (step.gate) {
            g.setColour (gateOnColour);
            g.fillRect (stepRect);
            // Inner highlight
            g.setColour (gateOnColour.withAlpha (0.3f));
            g.fillRect (stepRect.withSizeKeepingCentre (stepRect.getWidth() - 4,
                                                         stepRect.getHeight() - 4));
        } else {
            g.setColour (gateOffColour);
            g.fillRect (stepRect);
        }

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowGate) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawVelocityRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowVelocity, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        // Background cell
        g.setColour (gateOffColour);
        g.fillRect (stepRect);

        // Velocity bar - height proportional to velocity
        const int barH = juce::jlimit (2, rowHeight - 4,
                                       static_cast<int> ((rowHeight - 4) * step.velocity));
        const int barX = stepRect.getX() + (stepRect.getWidth() - columnWidth / 2) / 2;
        const int barY = stepRect.getBottom() - barH;
        g.setColour (velocityColour);
        g.fillRect (barX, barY, columnWidth / 2, barH);

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowVelocity) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawAccentRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowAccent, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        if (step.accent) {
            g.setColour (accentColour);
            g.fillRect (stepRect);
            // Inner highlight
            g.setColour (accentColour.withAlpha (0.3f));
            g.fillRect (stepRect.withSizeKeepingCentre (stepRect.getWidth() - 4,
                                                         stepRect.getHeight() - 4));
        } else {
            g.setColour (gateOffColour);
            g.fillRect (stepRect);
        }

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowAccent) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawProbabilityRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowProbability, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        // Background cell
        g.setColour (gateOffColour);
        g.fillRect (stepRect);

        // Probability bar - height proportional to probability
        const int barH = juce::jlimit (2, rowHeight - 4,
                                       static_cast<int> ((rowHeight - 4) * step.probability));
        const int barX = stepRect.getX() + (stepRect.getWidth() - columnWidth / 2) / 2;
        const int barY = stepRect.getBottom() - barH;
        g.setColour (juce::Colour::fromRGB (100, 200, 100)); // green for probability
        g.fillRect (barX, barY, columnWidth / 2, barH);

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowProbability) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawRatchetRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowRatchet, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        // Background cell
        g.setColour (gateOffColour);
        g.fillRect (stepRect);

        // Ratchet count drawn as blocks (1..4)
        const int count = juce::jlimit (1, 4, step.ratchetCount);
        const int blockW = (columnWidth - 8) / 4;
        for (int b = 0; b < count; ++b) {
            g.setColour (ratchetColour);
            g.fillRect (stepRect.getX() + 4 + b * blockW,
                        stepRect.getCentreY() - 4, blockW - 2, 8);
        }

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowRatchet) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawSlideRow (juce::Graphics& g)
{
    for (int i = 0; i < visibleSteps; ++i) {
        const auto stepRect = getStepRect (i, rowSlide, columnWidth, rowHeight, headerHeight, toolbarHeight);
        const auto& step = sequence[i];

        // Background cell
        g.setColour (gateOffColour);
        g.fillRect (stepRect);

        // Slide duration bar (0 = no slide)
        if (step.slideDuration > 0.0f) {
            const float ratio = juce::jlimit (0.0f, 1.0f, step.slideDuration / 4.0f);
            const int barH = juce::jlimit (2, rowHeight - 6,
                                           static_cast<int> ((rowHeight - 6) * ratio));
            g.setColour (slideColour);
            g.fillRect (stepRect.getX() + 4, stepRect.getBottom() - barH - 3,
                        columnWidth - 8, barH);
        }

        // Selected cell overlay
        if (i == selectedStep && selectedRow == rowSlide) {
            g.setColour (selectedCellOverlay);
            g.fillRect (stepRect);
        }

        // Play head highlight
        if (i == playHeadStep) {
            g.setColour (playHeadColour.withAlpha (0.15f));
            g.fillRect (stepRect);
        }
    }
}

void StepSequencer::drawPlayHead (juce::Graphics& g)
{
    if (playHeadStep < 0 || playHeadStep >= visibleSteps)
        return;

    const int x = playHeadStep * columnWidth;
    const auto startY = toolbarHeight;
    const auto endY = startY + headerHeight + numDataRows * rowHeight;
    g.setColour (playHeadColour);
    g.fillRect (x, startY, 2, endY - startY);
}

int StepSequencer::hitTestStep (const juce::MouseEvent& event) const noexcept
{
    if (event.y < toolbarHeight + headerHeight)
        return -1;
    return event.x / columnWidth;
}

int StepSequencer::hitTestRow (const juce::MouseEvent& event) const noexcept
{
    if (event.y < toolbarHeight + headerHeight)
        return -1;
    const int row = (event.y - toolbarHeight - headerHeight) / rowHeight;
    if (row < 0 || row >= numDataRows)
        return -1;
    return row;
}

void StepSequencer::handleRowAction (int step, int row, bool isMouseDown)
{
    if (step < 0 || step >= sequence.size())
        return;

    auto& s = sequence[step];

    switch (row) {
        case rowPitch:
            // Toggle: cycle through common intervals: 0, 12, -12, 7, -7, 5, -5
            if (s.noteOffset == 0) {
                s.noteOffset = 12;
            } else if (s.noteOffset == 12) {
                s.noteOffset = -12;
            } else if (s.noteOffset == -12) {
                s.noteOffset = 0;
            } else {
                s.noteOffset = 0;
            }
            break;
        case rowGate:
            s.gate = !s.gate;
            break;
        case rowVelocity:
            if (isMouseDown) {
                // Click sets velocity to high
                s.velocity = 1.0f;
            }
            break;
        case rowAccent:
            s.accent = !s.accent;
            break;
        case rowProbability:
            if (isMouseDown) {
                s.probability = 1.0f;
            }
            break;
        case rowRatchet:
            // Click cycles the ratchet count 1 -> 2 -> 3 -> 4 -> 1
            s.ratchetCount = s.ratchetCount >= 4 ? 1 : s.ratchetCount + 1;
            break;
        case rowSlide:
            // Click cycles slide duration 0 -> 1 -> 2 -> 3 -> 0 steps
            s.slideDuration = s.slideDuration >= 3.0f ? 0.0f : s.slideDuration + 1.0f;
            break;
        default:
            break;
    }
}

void StepSequencer::adjustWheelValue (int step, int row, bool forward)
{
    if (step < 0 || step >= sequence.size())
        return;

    auto& s = sequence[step];

    switch (row) {
        case rowPitch:
            s.noteOffset += forward ? 1 : -1;
            s.noteOffset = juce::jlimit (-24, 24, s.noteOffset);
            break;
        case rowVelocity:
            s.velocity += forward ? 0.05f : -0.05f;
            s.velocity = juce::jlimit (0.0f, 1.0f, s.velocity);
            break;
        case rowProbability:
            s.probability += forward ? 0.05f : -0.05f;
            s.probability = juce::jlimit (0.0f, 1.0f, s.probability);
            break;
        case rowRatchet:
            s.ratchetCount = juce::jlimit (1, 8, s.ratchetCount + (forward ? 1 : -1));
            break;
        case rowSlide:
            s.slideDuration = juce::jlimit (0.0f, 8.0f,
                s.slideDuration + (forward ? 1.0f : -1.0f));
            break;
        default:
            break;
    }
}

void StepSequencer::fireToolbarAction (ToolbarAction action)
{
    if (callbacks == nullptr)
        return;

    switch (action) {
        case ToolbarAction::copy:       callbacks->onCopy(); break;
        case ToolbarAction::paste:      callbacks->onPaste(); break;
        case ToolbarAction::rotateLeft: callbacks->onRotateLeft(); break;
        case ToolbarAction::rotateRight:callbacks->onRotateRight(); break;
        case ToolbarAction::reverse:    callbacks->onReverse(); break;
        case ToolbarAction::shiftLeft:  callbacks->onShiftLeft(); break;
        case ToolbarAction::shiftRight: callbacks->onShiftRight(); break;
        case ToolbarAction::transposeUp:callbacks->onTransposeUp(); break;
        case ToolbarAction::transposeDown:callbacks->onTransposeDown(); break;
        case ToolbarAction::octaveUp:   callbacks->onOctaveUp(); break;
        case ToolbarAction::octaveDown: callbacks->onOctaveDown(); break;
        case ToolbarAction::mutate:     callbacks->onMutate(); break;
        case ToolbarAction::clear:      callbacks->onClear(); break;
    }
}

void StepSequencer::resized()
{
    // Component handles its own size based on column count
}

void StepSequencer::mouseDown (const juce::MouseEvent& event)
{
    const int step = hitTestStep (event);
    const int row = hitTestRow (event);

    if (step >= 0 && row >= 0) {
        selectedStep = step;
        selectedRow = row;
        isDragging = true;
        dragStartStep = step;
        // Mirror the selection into the canonical sequence model so
        // mutateSelected / clearSelected operate on the clicked step.
        sequence.setSelectedRange (step, step);
        handleRowAction (step, row, true);
        repaint();
    }
}

void StepSequencer::mouseDrag (const juce::MouseEvent& event)
{
    if (!isDragging)
        return;

    const int step = hitTestStep (event);
    const int row = hitTestRow (event);

    if (step >= 0 && row >= 0) {
        selectedStep = step;
        selectedRow = row;
        if (step != dragStartStep || row != selectedRow) {
            sequence.setSelectedRange (step, step);
            handleRowAction (step, row, false);
            repaint();
        }
    }
}

void StepSequencer::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
    dragStartStep = -1;
}

void StepSequencer::mouseWheelMove (const juce::MouseEvent& event,
                                     const juce::MouseWheelDetails& details)
{
    const int step = hitTestStep (event);
    const int row = hitTestRow (event);

    if (step >= 0 && row >= 0) {
        const bool forward = details.deltaY > 0;
        adjustWheelValue (step, row, forward);
        repaint();
    }
}

} // namespace vstengine::gui
