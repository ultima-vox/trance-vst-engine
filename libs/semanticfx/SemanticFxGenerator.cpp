#include "semanticfx/SemanticFxGenerator.h"
#include "instrument/InstrumentContract.h"
#include <algorithm>

namespace vstengine::semanticfx {
namespace {
class Random final {
public:
    explicit Random(std::uint32_t seed) noexcept : state_(seed ? seed : 1u) {}
    std::uint32_t next() noexcept
    {
        auto value = state_;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        return state_ = value;
    }
    float unit() noexcept
    {
        return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f);
    }
private:
    std::uint32_t state_;
};
} // namespace

sequence::Sequence SemanticFxGenerator::generate(
    SemanticFxProfile profile, std::uint32_t globalSeed,
    std::uint64_t slotId, std::string_view instrumentId) noexcept
{
    const bool fast = profile == SemanticFxProfile::laser
        || profile == SemanticFxProfile::zap
        || profile == SemanticFxProfile::metallic;
    sequence::Sequence result(fast ? 32 : 16);
    result.setTimingMode(fast ? sequence::TimingMode::thirtySecond
                              : sequence::TimingMode::sixteenth);
    Random random(instrument::generationSubSeed(globalSeed, slotId,
        instrumentId) ^ (0x9e3779b9u
        * (1u + static_cast<std::uint32_t>(profile))));
    for (int index = 0; index < result.size(); ++index) {
        auto& step = result[index];
        step = {};
        step.probability = 1.0f;
        step.ratchetCount = 1;
        step.gateWidth = 0.12f;
        bool gate = false;
        switch (profile) {
            case SemanticFxProfile::sweep: gate = index == 0 || index == 12; break;
            case SemanticFxProfile::laser: gate = index % 7 == 1; break;
            case SemanticFxProfile::riser: gate = index == 0; break;
            case SemanticFxProfile::downlifter: gate = index == 8; break;
            case SemanticFxProfile::impact: gate = index == 0 || index == 8; break;
            case SemanticFxProfile::whoosh: gate = index == 3 || index == 11; break;
            case SemanticFxProfile::zap: gate = index % 5 == 2; break;
            case SemanticFxProfile::noiseBurst: gate = index % 4 == 3; break;
            case SemanticFxProfile::metallic: gate = index % 6 == 0; break;
            case SemanticFxProfile::alien:
                gate = (index == 2 || index == 7 || index == 13); break;
        }
        step.gate = gate;
        if (!gate) continue;
        step.noteOffset = static_cast<int>(random.next() % 13u) - 6;
        step.velocity = std::clamp(0.62f + random.unit() * 0.38f,
                                   0.0f, 1.0f);
        step.accent = step.velocity > 0.86f;
        step.probability = profile == SemanticFxProfile::alien
            || profile == SemanticFxProfile::noiseBurst ? 0.82f : 1.0f;
        step.ratchetCount = profile == SemanticFxProfile::zap
            && (index % 10 == 2) ? 2 : 1;
    }
    return result;
}

} // namespace vstengine::semanticfx
