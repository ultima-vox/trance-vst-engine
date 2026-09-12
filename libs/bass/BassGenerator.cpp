#include "bass/BassGenerator.h"
#include "instrument/InstrumentContract.h"
#include <algorithm>
#include <array>

namespace vstengine::bass {
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

sequence::Sequence BassGenerator::generate(
    BassProfile profile, std::uint32_t globalSeed, std::uint64_t slotId,
    std::string_view instrumentId) noexcept
{
    sequence::Sequence result(16);
    result.setTimingMode(sequence::TimingMode::sixteenth);
    Random random(instrument::generationSubSeed(globalSeed, slotId,
        instrumentId) ^ (0x27d4eb2du
        * (1u + static_cast<std::uint32_t>(profile))));
    constexpr std::array<int, 5> offsets { 0, 0, 0, 3, -2 };
    for (int index = 0; index < result.size(); ++index) {
        auto& step = result[index];
        step = {};
        step.gate = profile == BassProfile::psy ? index % 4 != 0
            : profile == BassProfile::forest ? random.unit() > 0.26f
            : index % 8 == 3 || index % 8 == 6 || random.unit() > 0.72f;
        step.noteOffset = offsets[random.next() % offsets.size()];
        step.accent = step.gate && (index % 8 == 1
            || (profile == BassProfile::forest && random.unit() < 0.16f));
        step.velocity = std::clamp(0.68f + (step.accent ? 0.22f
            : random.unit() * 0.12f), 0.0f, 1.0f);
        step.probability = step.gate && profile == BassProfile::forest
            && random.unit() < 0.2f ? 0.78f : 1.0f;
        step.ratchetCount = 1;
        step.gateWidth = profile == BassProfile::dark ? 0.46f : 0.58f;
    }
    return result;
}
} // namespace vstengine::bass
