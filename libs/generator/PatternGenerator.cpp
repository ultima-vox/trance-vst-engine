#include "PatternGenerator.h"
#include <random>

namespace vstengine::generator {
Pattern PatternGenerator::generate(const Style style, const std::uint32_t seed)
{
    std::mt19937 rng { seed };
    std::uniform_real_distribution<float> chance { 0.0f, 1.0f };
    Pattern pattern {};

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        auto& step = pattern[i];
        step.gate = (i % 4) != 0;
        step.accent = (i % 4) == 1;

        if (style == Style::darkPsy && step.gate && chance(rng) < 0.12f)
            step.noteOffset = 12;
        else if (style == Style::forest && step.gate) {
            if (chance(rng) < 0.18f)
                step.noteOffset = chance(rng) < 0.5f ? -12 : 12;
            if (chance(rng) < 0.08f)
                step.gate = false;
        } else if (style == Style::psytrance && step.gate && chance(rng) < 0.06f)
            step.noteOffset = 12;
    }
    return pattern;
}

Pattern PatternGenerator::generate(const vstengine::parts::PartId part,
                                   const Style style,
                                   const std::uint32_t globalSeed)
{
    return generate(style, effectiveSeed(part, globalSeed));
}
} // namespace vstengine::generator
