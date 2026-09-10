#pragma once
#include <array>
#include <cstdint>
#include "parts/Part.h"

namespace vstengine::generator {
struct PatternStep {
    bool gate {};
    bool accent {};
    int noteOffset {};
};
using Pattern = std::array<PatternStep, 16>;
enum class Style { darkPsy, psytrance, forest };

class PatternGenerator {
public:
    [[nodiscard]] static Pattern generate(Style style, std::uint32_t seed);
    [[nodiscard]] static Pattern generate(vstengine::parts::PartId part,
                                          Style style,
                                          std::uint32_t globalSeed);
    [[nodiscard]] static constexpr std::uint32_t effectiveSeed(
        vstengine::parts::PartId part, std::uint32_t globalSeed) noexcept
    {
        const auto stableHash = part == vstengine::parts::PartId::bass
            ? 0xB455A11u : 0xC1C4D00Du;
        return globalSeed ^ stableHash;
    }
};
} // namespace vstengine::generator
