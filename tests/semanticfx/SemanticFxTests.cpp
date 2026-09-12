#include "instrument/InstrumentComplianceHarness.h"
#include "semanticfx/SemanticFxEngine.h"
#include "semanticfx/SemanticFxGenerator.h"
#include "semanticfx/SemanticFxProvider.h"
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

double energy(const std::vector<float>& audio)
{
    return std::inner_product(audio.begin(), audio.end(), audio.begin(), 0.0);
}

std::uint64_t signature(const vstengine::sequence::Sequence& pattern)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (int index = 0; index < pattern.size(); ++index) {
        const auto& step = pattern[index];
        hash ^= static_cast<std::uint64_t>(step.gate)
            | (static_cast<std::uint64_t>(step.noteOffset + 64) << 2)
            | (static_cast<std::uint64_t>(step.velocity) << 12)
            | (static_cast<std::uint64_t>(step.ratchetCount) << 20);
        hash *= 1099511628211ULL;
    }
    return hash;
}
} // namespace

int main()
{
    using namespace vstengine;
    auto provider = semanticfx::createSemanticFxProvider();
    const auto compliance = tests::runInstrumentCompliance(
        *provider, semanticfx::instrumentId, { 60, 118 });
    if (!compliance)
        for (const auto& failure : compliance.failures)
            std::cerr << "COMPLIANCE: " << failure << '\n';
    REQUIRE(compliance.passed(), "Semantic FX passes common compliance");
    REQUIRE(provider->contentDescriptors(semanticfx::instrumentId).size() == 30,
            "ten semantic families expose sound/pattern/generator content");
    REQUIRE(provider->descriptors().front().budget.maxVoices
                == semanticfx::SemanticFxEngine::maximumVoices,
            "Semantic FX voice budget matches implementation");

    constexpr std::array profiles { semanticfx::SemanticFxProfile::sweep,
        semanticfx::SemanticFxProfile::laser,
        semanticfx::SemanticFxProfile::riser,
        semanticfx::SemanticFxProfile::downlifter,
        semanticfx::SemanticFxProfile::impact,
        semanticfx::SemanticFxProfile::whoosh,
        semanticfx::SemanticFxProfile::zap,
        semanticfx::SemanticFxProfile::noiseBurst,
        semanticfx::SemanticFxProfile::metallic,
        semanticfx::SemanticFxProfile::alien };
    std::set<std::uint64_t> signatures;
    for (const auto profile : profiles) {
        const auto a = semanticfx::SemanticFxGenerator::generate(profile, 991u,
            instrument::initialSlotId(5), semanticfx::instrumentId);
        const auto b = semanticfx::SemanticFxGenerator::generate(profile, 991u,
            instrument::initialSlotId(5), semanticfx::instrumentId);
        REQUIRE(signature(a) == signature(b),
                "Semantic FX generation deterministic");
        signatures.insert(signature(a));
    }
    REQUIRE(signatures.size() == profiles.size(),
            "semantic family generators materially differ");

    auto instance = provider->create(semanticfx::instrumentId,
        { instrument::initialSlotId(5), nullptr });
    REQUIRE(instance && instance->prepare({ 48000.0, 1024, 2 }),
            "Semantic FX instance prepares");
    std::vector<float> left(1024), right(1024);
    std::array<float*, 2> outputs { left.data(), right.data() };
    const std::array events {
        VoxMidiEventV1 { 0, 3, { 0x90, 60, 120 } },
        VoxMidiEventV1 { 1, 3, { 0x80, 60, 0 } }
    };
    instance->process({ outputs, 1024, events, 48000.0, 140.0, 0.0, true });
    REQUIRE(energy(left) > 1.0e-6,
            "one-shot continues after immediate NoteOff");
    REQUIRE(std::all_of(left.begin(), left.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 1.0f;
            }), "Semantic FX one-shot finite and bounded");

    std::cout << "Semantic FX tests passed (" << checks << ", compliance "
              << compliance.checks << ")\n";
    return EXIT_SUCCESS;
}
