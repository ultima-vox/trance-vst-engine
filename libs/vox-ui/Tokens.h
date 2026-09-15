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
} // namespace colour

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

namespace size {
inline constexpr int controlHeight = 32;
inline constexpr int tabHeight = 36;
inline constexpr int knobSmall = 36;
inline constexpr int knobNormal = 48;
inline constexpr int knobLarge = 64;
inline constexpr int instrumentSlotMinHeight = 54;
inline constexpr int instrumentSlotMaxHeight = 58;
inline constexpr int referenceWidth = 1440;
inline constexpr int referenceHeight = 1080;
} // namespace size

} // namespace vox::ui::tokens
