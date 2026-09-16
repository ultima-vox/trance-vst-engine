#pragma once

#include <juce_graphics/juce_graphics.h>

namespace vox::ui::tokens {

namespace colour {
inline const auto background = juce::Colour::fromString ("FF06121D");
inline const auto panel = juce::Colour::fromString ("FF0B1925");
inline const auto panelRaised = juce::Colour::fromString ("FF102230");
inline const auto control = juce::Colour::fromString ("FF0A1722");
inline const auto graph = juce::Colour::fromString ("FF051019");

inline const auto border = juce::Colour::fromString ("FF1D3B50");
inline const auto borderSubtle = juce::Colour::fromString ("FF122C3D");

inline const auto accent = juce::Colour::fromString ("FF00DDF5");
inline const auto accentHover = juce::Colour::fromString ("FF42EEFF");
inline const auto accentDim = juce::Colour::fromString ("FF087B92");

inline const auto text = juce::Colour::fromString ("FFE9F3FA");
inline const auto textSecondary = juce::Colour::fromString ("FF9AB2C5");
inline const auto textMuted = juce::Colour::fromString ("FF587487");

inline const auto danger = juce::Colour::fromString ("FFE4425D");
inline const auto success = juce::Colour::fromString ("FF32D296");
inline const auto warning = juce::Colour::fromString ("FFE3B341");

// Semantic aliases. Keeping these separate from their current backing colours
// lets family-level meaning evolve without component-local rewrites.
inline const auto focus = accent;
inline const auto selection = accent;
inline const auto info = accentDim;
inline const auto clip = danger;
inline const auto overlay = juce::Colour::fromString ("B8000000");
} // namespace colour

namespace opacity {
inline constexpr float disabled = 0.35f;
inline constexpr float subtle = 0.60f;
inline constexpr float secondary = 0.75f;
inline constexpr float overlayScrim = 0.72f;
inline constexpr float graphGridMinor = 0.10f;
inline constexpr float graphGridMajor = 0.18f;
inline constexpr float graphFill = 0.10f;
inline constexpr float hoverFill = 0.12f;
inline constexpr float focusGlow = 0.20f;
} // namespace opacity

namespace spacing {
inline constexpr int xs = 4;
inline constexpr int sm = 8;
inline constexpr int md = 12;
inline constexpr int lg = 16;
inline constexpr int xl = 20;
inline constexpr int xxl = 24;
inline constexpr int xxxl = 32;
} // namespace spacing

namespace radius {
inline constexpr float small = 4.0f;
inline constexpr float medium = 6.0f;
inline constexpr float large = 8.0f;
} // namespace radius

namespace stroke {
inline constexpr float hairline = 1.0f;
inline constexpr float normal = 1.5f;
inline constexpr float strong = 2.0f;
inline constexpr float focus = 2.0f;
} // namespace stroke

namespace icon {
inline constexpr int small = 16;
inline constexpr int normal = 20;
inline constexpr int large = 24;
inline constexpr float stroke = 1.5f;
} // namespace icon

namespace size {
inline constexpr int controlHeightSmall = 28;
inline constexpr int controlHeight = 32;
inline constexpr int tabHeight = 36;
inline constexpr int menuRowHeight = 28;
inline constexpr int toolbarHeight = 36;
inline constexpr int sectionHeaderHeight = 32;
inline constexpr int scrollbarWidth = 10;
inline constexpr int focusInset = 2;
inline constexpr int hitTargetMin = 24;
inline constexpr int iconHitTargetMin = 28;

inline constexpr int knobSmall = 36;
inline constexpr int knobNormal = 48;
inline constexpr int knobLarge = 64;

inline constexpr int instrumentSlotMinHeight = 54;
inline constexpr int instrumentSlotMaxHeight = 58;

inline constexpr int referenceWidth = 1440;
inline constexpr int referenceHeight = 1080;
} // namespace size

namespace layout {
inline constexpr int panelPaddingCompact = 8;
inline constexpr int panelPadding = 12;
inline constexpr int panelPaddingLarge = 16;
inline constexpr int panelGapCompact = 8;
inline constexpr int panelGap = 12;
inline constexpr int sectionGap = 16;
} // namespace layout

namespace graph {
inline constexpr float traceWidth = 1.75f;
inline constexpr float nodeDiameter = 7.0f;
inline constexpr float gridMajorWidth = 1.0f;
inline constexpr float gridMinorWidth = 1.0f;
} // namespace graph

namespace motion {
inline constexpr int fastMs = 80;
inline constexpr int normalMs = 140;
inline constexpr int slowMs = 220;
} // namespace motion

namespace scale {
inline constexpr float x75 = 0.75f;
inline constexpr float x100 = 1.00f;
inline constexpr float x125 = 1.25f;
inline constexpr float x150 = 1.50f;
inline constexpr float x200 = 2.00f;
} // namespace scale

} // namespace vox::ui::tokens
