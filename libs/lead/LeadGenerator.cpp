#include "lead/LeadGenerator.h"
#include "instrument/InstrumentContract.h"
#include <algorithm>
#include <array>

namespace vstengine::lead {
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

constexpr std::array<int, 12> minorScale {
    0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27
};
constexpr std::array<int, 10> alienScale {
    0, 1, 5, 6, 10, 13, 17, 18, 22, 25
};

void defaults(sequence::Step& step) noexcept
{
    step = {};
    step.velocity = 0.76f;
    step.probability = 1.0f;
    step.ratchetCount = 1;
    step.gateWidth = 0.58f;
}
} // namespace

std::uint32_t LeadGenerator::stableSeed(std::uint32_t globalSeed,
                                        std::uint64_t slotId,
                                        std::string_view instrumentId) noexcept
{
    return instrument::generationSubSeed(globalSeed, slotId, instrumentId);
}

sequence::Sequence LeadGenerator::generate(LeadProfile profile,
                                            std::uint32_t globalSeed,
                                            std::uint64_t slotId,
                                            std::string_view instrumentId) noexcept
{
    const int length = profile == LeadProfile::hiTechBurst
        || profile == LeadProfile::metallicSequence ? 32 : 16;
    sequence::Sequence result(length);
    result.setTimingMode(profile == LeadProfile::hypnoticLead
        ? sequence::TimingMode::triplet
        : profile == LeadProfile::hiTechBurst
            ? sequence::TimingMode::thirtySecond
            : sequence::TimingMode::sixteenth);
    Random random(stableSeed(globalSeed, slotId, instrumentId)
        ^ (0x7f4a7c15u * (1u + static_cast<std::uint32_t>(profile))));

    int previousNote = 0;
    bool previousGate = false;
    for (int index = 0; index < length; ++index) {
        auto& step = result.getStep(index);
        defaults(step);
        const int beat = index % 4;
        switch (profile) {
            case LeadProfile::psyArp:
                step.gate = beat != 3 || random.unit() > 0.48f;
                step.noteOffset = minorScale[static_cast<std::size_t>(
                    (index * 2 + random.choose(4)) % 10)];
                step.accent = step.gate && (beat == 0 || random.unit() < 0.14f);
                step.gateWidth = 0.48f;
                break;
            case LeadProfile::forestCall:
                step.gate = index % 8 < 3 || random.unit() > 0.78f;
                step.noteOffset = minorScale[static_cast<std::size_t>(
                    3 + random.choose(9))];
                step.accent = step.gate && index % 8 == 0;
                step.gateWidth = 0.74f;
                break;
            case LeadProfile::alienPhrase:
                step.gate = random.unit() > (beat == 0 ? 0.48f : 0.25f);
                step.noteOffset = alienScale[static_cast<std::size_t>(random.choose(10))];
                step.accent = step.gate && random.unit() < 0.24f;
                step.probability = step.gate && random.unit() < 0.2f ? 0.76f : 1.0f;
                step.gateWidth = 0.38f + 0.48f * random.unit();
                break;
            case LeadProfile::metallicSequence:
                step.gate = random.unit() > 0.18f;
                step.noteOffset = alienScale[static_cast<std::size_t>(
                    (index + random.choose(5)) % 10)];
                step.accent = step.gate && index % 6 == 2;
                step.ratchetCount = step.gate && random.unit() < 0.12f ? 2 : 1;
                step.gateWidth = 0.32f;
                break;
            case LeadProfile::hiTechBurst:
                step.gate = index % 8 < 5 && random.unit() > 0.12f;
                step.noteOffset = minorScale[static_cast<std::size_t>(random.choose(12))];
                step.accent = step.gate && (index % 8 == 0 || random.unit() < 0.18f);
                step.ratchetCount = step.gate && index % 8 == 4 ? 3
                    : step.gate && random.unit() < 0.15f ? 2 : 1;
                step.gateWidth = 0.25f;
                break;
            case LeadProfile::hypnoticLead:
                step.gate = index % 3 != 2 || random.unit() > 0.68f;
                step.noteOffset = index % 6 < 3 ? 12
                    : (random.unit() < 0.5f ? 15 : 19);
                step.accent = step.gate && index % 6 == 0;
                step.gateWidth = 0.92f;
                break;
        }
        step.velocity = std::clamp(step.velocity + (step.accent ? 0.2f
            : random.unit() * 0.12f - 0.06f), 0.38f, 1.0f);
        if (step.gate && previousGate && step.noteOffset != previousNote) {
            const float slideChance = profile == LeadProfile::hypnoticLead ? 0.42f
                : profile == LeadProfile::alienPhrase ? 0.2f : 0.08f;
            if (random.unit() < slideChance) {
                step.slideDuration = profile == LeadProfile::hypnoticLead
                    ? 1.25f : 0.75f;
                step.gateWidth = 1.0f;
            }
        }
        previousGate = step.gate;
        if (step.gate) previousNote = step.noteOffset;
    }
    return result;
}

} // namespace vstengine::lead
