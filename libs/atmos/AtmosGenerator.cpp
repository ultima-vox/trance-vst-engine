#include "atmos/AtmosGenerator.h"
#include "instrument/InstrumentContract.h"
#include <algorithm>
#include <array>

namespace vstengine::atmos {
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
    int choose(int count) noexcept
    {
        return count > 0
            ? static_cast<int>(next() % static_cast<std::uint32_t>(count)) : 0;
    }
private:
    std::uint32_t state_;
};

constexpr std::array<int, 12> openMinor {
    -12, -5, 0, 3, 7, 10, 12, 15, 19, 22, 24, 31
};
constexpr std::array<int, 10> alien {
    -12, -6, 0, 1, 6, 7, 13, 18, 19, 25
};

void initialise(sequence::Step& step) noexcept
{
    step = {};
    step.velocity = 0.64f;
    step.probability = 1.0f;
    step.ratchetCount = 1;
    step.gateWidth = 1.0f;
}
} // namespace

std::uint32_t AtmosGenerator::stableSeed(std::uint32_t globalSeed,
                                         std::uint64_t slotId,
                                         std::string_view instrumentId) noexcept
{
    return instrument::generationSubSeed(globalSeed, slotId, instrumentId);
}

sequence::Sequence AtmosGenerator::generate(AtmosProfile profile,
                                             std::uint32_t globalSeed,
                                             std::uint64_t slotId,
                                             std::string_view instrumentId) noexcept
{
    sequence::Sequence result(64);
    result.setTimingMode(sequence::TimingMode::eighth);
    Random random(stableSeed(globalSeed, slotId, instrumentId)
        ^ (0x85ebca6bu * (1u + static_cast<std::uint32_t>(profile))));

    for (int index = 0; index < result.size(); ++index) {
        auto& step = result.getStep(index);
        initialise(step);
        bool trigger = false;
        switch (profile) {
            case AtmosProfile::deepSpace:
                trigger = index == 0 || index == 16 || index == 40;
                step.noteOffset = openMinor[static_cast<std::size_t>(
                    trigger ? random.choose(5) : 0)];
                step.velocity = 0.48f + 0.16f * random.unit();
                break;
            case AtmosProfile::forestBed:
                trigger = index % 8 == 0 && random.unit() > 0.22f;
                step.noteOffset = openMinor[static_cast<std::size_t>(
                    2 + random.choose(7))];
                step.velocity = 0.38f + 0.34f * random.unit();
                step.probability = trigger ? 0.84f : 1.0f;
                break;
            case AtmosProfile::alienDrone:
                trigger = index == 0 || index == 20 || index == 36 || index == 52;
                step.noteOffset = alien[static_cast<std::size_t>(random.choose(10))];
                step.velocity = 0.52f + 0.24f * random.unit();
                break;
            case AtmosProfile::crystalAir:
                trigger = index % 6 == 0 && random.unit() > 0.12f;
                step.noteOffset = openMinor[static_cast<std::size_t>(
                    5 + random.choose(7))];
                step.velocity = 0.32f + 0.34f * random.unit();
                step.probability = trigger && random.unit() < 0.3f ? 0.72f : 1.0f;
                break;
            case AtmosProfile::darkRitual:
                trigger = index % 16 == 0 || index == 26 || index == 58;
                step.noteOffset = openMinor[static_cast<std::size_t>(
                    random.choose(6))];
                step.velocity = 0.58f + 0.22f * random.unit();
                step.accent = trigger && index % 16 == 0;
                break;
            case AtmosProfile::evolvingCloud:
                trigger = index % 7 == 0 || (index % 11 == 3 && random.unit() > 0.45f);
                step.noteOffset = openMinor[static_cast<std::size_t>(
                    2 + random.choose(10))];
                step.velocity = 0.34f + 0.3f * random.unit();
                step.probability = trigger ? 0.78f + 0.2f * random.unit() : 1.0f;
                break;
        }
        step.gate = trigger;
        step.gateWidth = 1.0f;
        if (trigger && step.accent) step.velocity = std::min(1.0f,
            step.velocity + 0.15f);
    }
    return result;
}

} // namespace vstengine::atmos
