#include "atmos/AtmosEngine.h"
#include "atmos/AtmosGenerator.h"
#include "atmos/AtmosProvider.h"
#include "instrument/InstrumentComplianceHarness.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <set>
#include <vector>

namespace {
int checks {};
#define REQUIRE(condition, message) do { ++checks; if (!(condition)) { \
    std::cerr << "FAILED: " << message << " line " << __LINE__ << '\n'; \
    return EXIT_FAILURE; } } while (false)

std::uint64_t signature(const vstengine::sequence::Sequence& pattern)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (int index = 0; index < pattern.size(); ++index) {
        const auto& step = pattern[index];
        hash ^= static_cast<std::uint64_t>(step.gate)
            | (static_cast<std::uint64_t>(step.noteOffset + 64) << 2)
            | (static_cast<std::uint64_t>(step.velocity) << 12)
            | (static_cast<std::uint64_t>(step.gateWidth * 1000.0f) << 20);
        hash *= 1099511628211ULL;
    }
    return hash;
}
} // namespace

int main()
{
    using namespace vstengine;
    auto provider = atmos::createAtmosProvider();
    const auto compliance = tests::runInstrumentCompliance(
        *provider, atmos::instrumentId, { 48, 105 });
    if (!compliance)
        for (const auto& failure : compliance.failures)
            std::cerr << "COMPLIANCE: " << failure << '\n';
    REQUIRE(compliance.passed(), "Atmos passes common module compliance");
    REQUIRE(provider->contentDescriptors(atmos::instrumentId).size() == 18,
            "Atmos exposes six sound/pattern/generator profiles");

    constexpr std::array profiles { atmos::AtmosProfile::deepSpace,
        atmos::AtmosProfile::forestBed, atmos::AtmosProfile::alienDrone,
        atmos::AtmosProfile::crystalAir, atmos::AtmosProfile::darkRitual,
        atmos::AtmosProfile::evolvingCloud };
    std::set<std::uint64_t> signatures;
    for (const auto profile : profiles) {
        const auto a = atmos::AtmosGenerator::generate(profile, 8128u,
            instrument::initialSlotId(6), atmos::instrumentId);
        const auto b = atmos::AtmosGenerator::generate(profile, 8128u,
            instrument::initialSlotId(6), atmos::instrumentId);
        REQUIRE(a.size() == 64, "Atmos generator uses long-form 64-step canvas");
        REQUIRE(signature(a) == signature(b),
                "Atmos generator deterministic");
        signatures.insert(signature(a));
    }
    REQUIRE(signatures.size() == profiles.size(),
            "Atmos generator profiles materially differ");

    auto instance = provider->create(atmos::instrumentId,
        { instrument::initialSlotId(6), nullptr });
    REQUIRE(instance && instance->prepare({ 48000.0, 4096, 2 }),
            "Atmos instance prepares");
    std::set<long long> energies;
    for (const auto& content : provider->contentDescriptors(atmos::instrumentId)) {
        if (content.kind != instrument::ContentKind::soundPreset) continue;
        REQUIRE(provider->applySoundPreset(atmos::instrumentId, content.id,
                    *instance) == instrument::ContentStatus::ok,
                "Atmos sound preset applies transactionally");
        instance->reset();
        std::vector<float> left(4096), right(4096);
        std::array<float*, 2> outputs { left.data(), right.data() };
        const std::array midi { VoxMidiEventV1 { 0, 3, { 0x90, 48, 108 } } };
        instance->process({ outputs, 4096, midi, 48000.0, 128.0, 0.0, true });
        REQUIRE(std::all_of(left.begin(), left.end(), [](float sample) {
                    return std::isfinite(sample) && std::abs(sample) <= 1.0f;
                }), "Atmos render finite and bounded");
        const auto energy = std::inner_product(left.begin(), left.end(),
                                               left.begin(), 0.0);
        REQUIRE(energy > 1.0e-6, "Atmos preset audible");
        energies.insert(static_cast<long long>(energy * 100000.0));
    }
    REQUIRE(energies.size() >= 4,
            "Atmos sound presets produce materially different renders");

    std::cout << "Atmos tests passed (" << checks << ", compliance "
              << compliance.checks << ")\n";
    return EXIT_SUCCESS;
}
