#include "ZoneRangeEditor.h"
#include "instrument/HostParameterSchema.h"
#include <cmath>
#include <cstdio>

namespace vstengine::ui {
namespace {
std::string id(std::size_t slot, std::string_view suffix)
{
    char result[24] {};
    std::snprintf(result, sizeof(result), "slot%02zu%.*s", slot + 1,
                  static_cast<int>(suffix.size()), suffix.data());
    return result;
}
std::size_t index(ZoneRangeEditor::Target target) noexcept
{
    return static_cast<std::size_t>(target) - 1u;
}
} // namespace

ZoneRangeEditor::ZoneRangeEditor(juce::AudioProcessorValueTreeState& value)
    : state(value)
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    startTimerHz(15);
}

void ZoneRangeEditor::bind(std::size_t slot)
{
    constexpr std::array suffixes {
        "KeyLow", "KeyHigh", "VelocityLow", "VelocityHigh"
    };
    for (std::size_t item = 0; item < suffixes.size(); ++item) {
        const auto parameterId = id(slot, suffixes[item]);
        parameters[item] = state.getParameter(parameterId);
        values[item] = state.getRawParameterValue(parameterId);
    }
    repaint();
}

juce::Rectangle<float> ZoneRangeEditor::keyBounds() const noexcept
{
    return getLocalBounds().toFloat().reduced(10.0f).withTrimmedTop(28.0f)
        .removeFromTop(74.0f);
}

juce::Rectangle<float> ZoneRangeEditor::velocityBounds() const noexcept
{
    auto area = getLocalBounds().toFloat().reduced(10.0f);
    area.removeFromTop(130.0f);
    return area.removeFromTop(28.0f);
}

float ZoneRangeEditor::value(Target which) const noexcept
{
    if (which == Target::none) return 0.0f;
    const auto* pointer = values[index(which)];
    return pointer != nullptr ? pointer->load(std::memory_order_relaxed) : 0.0f;
}

void ZoneRangeEditor::paint(juce::Graphics& g)
{
    auto panel = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colours::panel);
    g.fillRoundedRectangle(panel, metrics::corner);
    g.setColour(colours::border);
    g.drawRoundedRectangle(panel, metrics::corner, 1.0f);
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(colours::mutedText);
    g.drawText("Key range", 12, 8, 120, 20, juce::Justification::left);
    const auto keys = keyBounds();
    const auto keyLow = value(Target::keyLow);
    const auto keyHigh = value(Target::keyHigh);
    for (int note = 0; note < 128; ++note) {
        const auto x0 = keys.getX() + keys.getWidth() * note / 128.0f;
        const auto x1 = keys.getX() + keys.getWidth() * (note + 1) / 128.0f;
        const auto pitch = note % 12;
        const bool black = pitch == 1 || pitch == 3 || pitch == 6
            || pitch == 8 || pitch == 10;
        g.setColour(black ? colours::inactive : colours::text.darker(0.18f));
        g.fillRect(juce::Rectangle<float>(x0, keys.getY(),
                   juce::jmax(1.0f, x1 - x0 - 0.25f),
                   black ? keys.getHeight() * 0.62f : keys.getHeight()));
    }
    const auto activeX = keys.getX() + keys.getWidth() * keyLow / 127.0f;
    const auto activeRight = keys.getX()
        + keys.getWidth() * keyHigh / 127.0f;
    g.setColour(colours::primary.withAlpha(0.34f));
    g.fillRect(juce::Rectangle<float>(activeX, keys.getY(),
        juce::jmax(2.0f, activeRight - activeX), keys.getHeight()));
    g.setColour(colours::primary);
    g.fillRect(activeX - 1.0f, keys.getY(), 2.0f, keys.getHeight());
    g.fillRect(activeRight - 1.0f, keys.getY(), 2.0f, keys.getHeight());
    g.setColour(colours::text);
    g.drawText(juce::MidiMessage::getMidiNoteName(
                   juce::roundToInt(keyLow), true, true, 3)
                   + "  –  "
                   + juce::MidiMessage::getMidiNoteName(
                       juce::roundToInt(keyHigh), true, true, 3),
               keys.toNearestInt().translated(0, -24),
               juce::Justification::centredRight);

    const auto velocity = velocityBounds();
    const auto velocityLow = value(Target::velocityLow);
    const auto velocityHigh = value(Target::velocityHigh);
    g.setColour(colours::mutedText);
    g.drawText("Velocity range", 12, static_cast<int>(velocity.getY()) - 24,
               120, 20, juce::Justification::left);
    g.setColour(colours::inactive);
    g.fillRoundedRectangle(velocity, 5.0f);
    const auto vx0 = velocity.getX() + velocity.getWidth()
        * (velocityLow - 1.0f) / 126.0f;
    const auto vx1 = velocity.getX() + velocity.getWidth()
        * (velocityHigh - 1.0f) / 126.0f;
    g.setColour(colours::primary.withAlpha(0.55f));
    g.fillRoundedRectangle({ vx0, velocity.getY(), juce::jmax(3.0f, vx1-vx0),
                             velocity.getHeight() }, 5.0f);
    for (const auto x : { vx0, vx1 }) {
        g.setColour(colours::text);
        g.fillEllipse(x - 6.0f, velocity.getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour(colours::primary);
        g.fillEllipse(x - 3.0f, velocity.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
    g.setColour(colours::text);
    g.drawText(juce::String(juce::roundToInt(velocityLow)) + "  –  "
                   + juce::String(juce::roundToInt(velocityHigh)),
               velocity.toNearestInt().translated(0, -24),
               juce::Justification::centredRight);
}

void ZoneRangeEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto point = event.position;
    if (keyBounds().contains(point)) {
        const auto note = juce::jlimit(0.0f, 127.0f,
            127.0f * (point.x - keyBounds().getX()) / keyBounds().getWidth());
        target = std::abs(note - value(Target::keyLow))
            <= std::abs(note - value(Target::keyHigh))
            ? Target::keyLow : Target::keyHigh;
    } else if (velocityBounds().contains(point)) {
        const auto velocity = juce::jlimit(1.0f, 127.0f,
            1.0f + 126.0f * (point.x - velocityBounds().getX())
                / velocityBounds().getWidth());
        target = std::abs(velocity - value(Target::velocityLow))
            <= std::abs(velocity - value(Target::velocityHigh))
            ? Target::velocityLow : Target::velocityHigh;
    }
    if (target != Target::none && parameters[index(target)] != nullptr)
        parameters[index(target)]->beginChangeGesture();
    update(point);
}

void ZoneRangeEditor::mouseDrag(const juce::MouseEvent& event)
{
    update(event.position);
}

void ZoneRangeEditor::mouseUp(const juce::MouseEvent&)
{
    if (target != Target::none && parameters[index(target)] != nullptr)
        parameters[index(target)]->endChangeGesture();
    target = Target::none;
}

void ZoneRangeEditor::update(juce::Point<float> point)
{
    if (target == Target::none) return;
    float plain {};
    if (target == Target::keyLow || target == Target::keyHigh) {
        const auto bounds = keyBounds();
        plain = std::round(juce::jlimit(0.0f, 127.0f,
            127.0f * (point.x - bounds.getX()) / bounds.getWidth()));
        if (target == Target::keyLow) plain = std::min(plain,
            value(Target::keyHigh));
        else plain = std::max(plain, value(Target::keyLow));
    } else {
        const auto bounds = velocityBounds();
        plain = std::round(juce::jlimit(1.0f, 127.0f,
            1.0f + 126.0f * (point.x - bounds.getX()) / bounds.getWidth()));
        if (target == Target::velocityLow) plain = std::min(plain,
            value(Target::velocityHigh));
        else plain = std::max(plain, value(Target::velocityLow));
    }
    auto* parameter = parameters[index(target)];
    if (parameter != nullptr)
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
    repaint();
}
} // namespace vstengine::ui
