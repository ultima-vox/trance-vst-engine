#pragma once

#include "common/UiComponents.h"
#include <array>

namespace vstengine::ui {
class ZoneRangeEditor final : public juce::Component, private juce::Timer {
public:
    enum class Target { none, keyLow, keyHigh, velocityLow, velocityHigh };
    explicit ZoneRangeEditor(juce::AudioProcessorValueTreeState&);
    void bind(std::size_t slotIndex);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    [[nodiscard]] juce::Rectangle<float> keyBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<float> velocityBounds() const noexcept;
    [[nodiscard]] float value(Target) const noexcept;
    void update(juce::Point<float>);
    juce::AudioProcessorValueTreeState& state;
    std::array<juce::RangedAudioParameter*, 4> parameters {};
    std::array<std::atomic<float>*, 4> values {};
    Target target { Target::none };
};
} // namespace vstengine::ui
