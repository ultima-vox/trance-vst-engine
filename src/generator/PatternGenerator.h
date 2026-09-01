#pragma once
#include <array>
#include <cstdint>

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
};
} // namespace vstengine::generator
