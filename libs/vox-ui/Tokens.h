#pragma once

#include <juce_graphics/juce_graphics.h>

namespace vox::ui::tokens {

// VOX UI Foundation Tokens v1.1
// Family-wide semantic tokens only. Product UI must consume these tokens rather
// than defining private palettes, spacing scales or interaction geometry.

namespace colour {
inline const auto background = juce::Colour::fromString ("FF06121D");
inline const auto panel = juce::Colour::fromString ("FF0B1925");
inline const auto panelRaised = juce::Colour::fromString ("FF102230");
inline const auto control = juce::Colour::fromString ("FF0A1722");
inline const auto controlHover = juce::Colour::fromString ("FF0E2130");
inline const auto graph = juce::Colour::fromString ("FF051019");
inline const auto overlay = juce::Colour::fromString ("CC020910");

inline const auto border = juce::Colour::fromString ("FF1D3B50");
inline const auto borderSubtle = juce::Colour::fromString ("FF122C3D");
inline const auto borderStrong = juce::Colour::fromString ("FF2B5873");

inline const auto accent = juce::Colour::fromString ("FF00DDF5");
inline const auto accentHover = juce::Colour::fromString ("FF42EEFF");
inline const auto accentDim = juce::Colour::fromString ("FF087B92");
inline const auto accentMuted = juce::Colour::fromString ("FF0B4655");

inline const auto text = juce::Colour::fromString ("FFE9F3FA");
inline const auto textSecondary = juce::Colour::fromString ("FF9AB2C5");
inline const auto textMuted = juce::Colour::fromString ("FF587487");
inline const auto textInverse = juce::Colour::fromString ("FF06121D");

inline const auto danger = juce::Colour::fromString ("FFE4425D");
inline const auto success = juce::Colour::fromString ("FF32D296");
inline const auto warning = juce::Colour::fromString ("FFE3B341");

inline const auto meterLow = success;
inline const auto meterMid = warning;
inline const auto meterHigh = danger;
inline const auto meterTrack = borderSubtle;
inline const auto graphGrid = borderSubtle;
inline const auto graphCurve = accent;
inline const auto focusRing = accent;
} // namespace colour

namespace spacing {
inline constexpr int none = 0;
inline constexpr int xs = 4;
inline constexpr int sm = 8;
inline constexpr int md = 12;
inline constexpr int lg = 16;
inline constexpr int xl = 20;
inline constexpr int xxl = 24;
inline constexpr int xxxl = 32;
inline constexpr int huge = 40;
inline constexpr int page = 16;
inline constexpr int panel = 12;
inline constexpr int sectionGap = 12;
inline constexpr int controlGap = 8;
} // namespace spacing

namespace radius {
inline constexpr float none = 0.0f;
inline constexpr float small = 4.0f;
inline constexpr float medium = 6.0f;
inline constexpr float large = 8.0f;
} // namespace radius

namespace stroke {
inline constexpr float hairline = 1.0f;
inline constexpr float control = 1.0f;
inline constexpr float focus = 1.5f;
inline constexpr float graph = 1.75f;
inline constexpr float graphStrong = 2.0f;
inline constexpr float icon = 1.75f;
} // namespace stroke

namespace opacity {
inline constexpr float disabled = 0.35f;
inline constexpr float muted = 0.60f;
inline constexpr float secondary = 0.78f;
inline constexpr float grid = 0.42f;
inline constexpr float hoverOverlay = 0.08f;
inline constexpr float pressedOverlay = 0.14f;
inline constexpr float glow = 0.22f;
inline constexpr float full = 1.0f;
} // namespace opacity

namespace size {
inline constexpr int controlHeight = 32;
inline constexpr int compactControlHeight = 28;
inline constexpr int tabHeight = 36;
inline constexpr int topBarHeight = 48;
inline constexpr int partHeaderHeight = 56;
inline constexpr int keyboardMinHeight = 112;
inline constexpr int keyboardPreferredHeight = 132;
inline constexpr int knobSmall = 36;
inline constexpr int knobNormal = 48;
inline constexpr int knobLarge = 64;
inline constexpr int instrumentSlotMinHeight = 54;
inline constexpr int instrumentSlotMaxHeight = 58;
inline constexpr int iconSmall = 12;
inline constexpr int iconNormal = 16;
inline constexpr int iconLarge = 20;
inline constexpr int minHitTarget = 28;
inline constexpr int referenceWidth = 1440;
inline constexpr int referenceHeight = 1080;
} // namespace size

namespace focus {
inline constexpr float ringWidth = stroke::focus;
inline constexpr float ringInset = 1.0f;
inline constexpr float ringGap = 1.0f;
} // namespace focus

namespace graph {
inline constexpr float curveWidth = stroke::graph;
inline constexpr float strongCurveWidth = stroke::graphStrong;
inline constexpr float nodeDiameter = 7.0f;
inline constexpr float nodeHitDiameter = 16.0f;
inline constexpr float gridAlpha = opacity::grid;
} // namespace graph

namespace meter {
inline constexpr int minWidth = 6;
inline constexpr int preferredWidth = 8;
inline constexpr int peakHoldMs = 900;
inline constexpr int peakFallMs = 1400;
} // namespace meter

namespace interaction {
inline constexpr double fineAdjustMultiplier = 0.10;
inline constexpr double wheelStepMultiplier = 0.02;
inline constexpr int tooltipDelayMs = 650;
inline constexpr int hoverTransitionMs = 90;
inline constexpr int stateTransitionMs = 120;
inline constexpr int pageTransitionMs = 140;
} // namespace interaction

namespace elevation {
// JUCE has no mandatory material-style elevation model. These values define
// hierarchy only; they must not be interpreted as permission for heavy shadows.
inline constexpr int base = 0;
inline constexpr int raised = 1;
inline constexpr int overlay = 2;
inline constexpr int modal = 3;
} // namespace elevation

} // namespace vox::ui::tokens
