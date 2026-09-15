#pragma once
#include <juce_graphics/juce_graphics.h>

namespace vox::ui::typography {

inline juce::Font instrumentTitle() { return juce::Font (juce::FontOptions (26.0f).withStyle ("SemiBold")); }
inline juce::Font sectionTitle()    { return juce::Font (juce::FontOptions (14.0f).withStyle ("SemiBold")); }
inline juce::Font controlLabel()    { return juce::Font (juce::FontOptions (12.0f)); }
inline juce::Font valueText()       { return juce::Font (juce::FontOptions (11.0f)); }

// канонические размеры для справки
inline constexpr float instrumentTitlePx = 26.0f;
inline constexpr float sectionTitlePx    = 14.0f;
inline constexpr float controlLabelPx    = 12.0f;
inline constexpr float valueTextPx       = 11.0f;

} // namespace vox::ui::typography
