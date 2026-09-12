#include "instrument/InstrumentComplianceHarness.h"
#include "lead/LeadEngine.h"
#include "lead/LeadGenerator.h"
#include "lead/LeadProvider.h"
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

double energy(const std::vector<float>& audio)
{
    return std::inner_product(audio.begin(), audio.end(), audio.begin(), 0.0);
}

std::vector<float> render(vstengine::instrument::InstrumentInstance& instance,
                          int note = 72, int velocity = 112)
{
    constexpr std::uint32_t samples = 4096;
    std::vector<float> left(samples), right(samples);
    std::array<float*, 2> outputs { left.data(), right.data() };
    const std::array midi { VoxMidiEventV1 { 0, 3,
        { 0x90, static_cast<std::uint8_t>(note),
          static_cast<std::uint8_t>(velocity) } } };
    instance.process({ outputs, samples, midi, 48000.0, 142.0, 0.0, true });
    return left;
}

std::uint64_t signature(const vstengine::sequence::Sequence& pattern)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (int index = 0; index < pattern.size(); ++index) {
        const auto& step = pattern[index];
        hash ^= static_cast<std::uint64_t>(step.gate)
            | (static_cast<std::uint64_t>(step.accent) << 1)
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
    auto provider = lead::createLeadProvider();
    const auto compliance = tests::runInstrumentCompliance(
        *provider, lead::instrumentId, { 72, 110 });
    if (!compliance)
        for (const auto& failure : compliance.failures)
            std::cerr << "COMPLIANCE: " << failure << '\n';
    REQUIRE(compliance.passed(), "Lead passes common module compliance");

    REQUIRE(provider->descriptors().size() == 1,
            "Lead provider exposes one instrument");
    const auto& descriptor = provider->descriptors().front();
    REQUIRE(descriptor.id == lead::instrumentId,
            "Lead durable InstrumentId");
    REQUIRE(descriptor.budget.maxVoices == lead::LeadEngine::maximumVoices,
            "Lead voice budget matches engine");
    REQUIRE((descriptor.capabilities & instrument::Capability::sequence) != 0
            && (descriptor.capabilities
                & instrument::Capability::patternGenerator) != 0,
            "Lead declares sequence/generator capabilities");
    REQUIRE(provider->contentDescriptors(lead::instrumentId).size() == 18,
            "Lead exposes six sound/pattern/generator variants");

    constexpr std::array profiles { lead::LeadProfile::psyArp,
        lead::LeadProfile::forestCall, lead::LeadProfile::alienPhrase,
        lead::LeadProfile::metallicSequence, lead::LeadProfile::hiTechBurst,
        lead::LeadProfile::hypnoticLead };
    std::set<std::uint64_t> signatures;
    for (const auto profile : profiles) {
        const auto first = lead::LeadGenerator::generate(profile, 0x9a31u,
            instrument::initialSlotId(3), lead::instrumentId);
        const auto second = lead::LeadGenerator::generate(profile, 0x9a31u,
            instrument::initialSlotId(3), lead::instrumentId);
        REQUIRE(signature(first) == signature(second),
                "Lead generator deterministic after reset");
        signatures.insert(signature(first));
    }
    REQUIRE(signatures.size() == profiles.size(),
            "Lead generator profiles materially differ");
    REQUIRE(signature(lead::LeadGenerator::generate(profiles[0], 0x9a31u,
                instrument::initialSlotId(3), lead::instrumentId))
            != signature(lead::LeadGenerator::generate(profiles[0], 0x9a31u,
                instrument::initialSlotId(4), lead::instrumentId)),
            "Lead generation includes SlotId domain");

    auto instance = provider->create(lead::instrumentId,
        { instrument::initialSlotId(3), nullptr });
    REQUIRE(instance && instance->prepare({ 48000.0, 4096, 2 }),
            "Lead instance prepares");
    std::set<long long> presetEnergy;
    for (const auto& content : provider->contentDescriptors(lead::instrumentId)) {
        if (content.kind != instrument::ContentKind::soundPreset) continue;
        REQUIRE(provider->applySoundPreset(lead::instrumentId, content.id,
                    *instance) == instrument::ContentStatus::ok,
                "Lead sound preset applies transactionally");
        instance->reset();
        const auto audio = render(*instance);
        REQUIRE(std::all_of(audio.begin(), audio.end(), [](float sample) {
                    return std::isfinite(sample) && std::abs(sample) <= 1.0f;
                }), "Lead render finite and bounded");
        presetEnergy.insert(static_cast<long long>(energy(audio) * 100000.0));
    }
    REQUIRE(presetEnergy.size() >= 4,
            "Lead sound presets produce materially different renders");

    VoxGenerationContextV1 context {};
    context.structSize = sizeof(context);
    context.globalSeed = 771u;
    context.slotId = instrument::initialSlotId(3);
    VoxPatternV1 output {};
    output.structSize = sizeof(output);
    REQUIRE(provider->generatePattern(lead::instrumentId, "psy-arp", context,
                nullptr, output) == instrument::ContentStatus::ok,
            "Lead POD pattern generation succeeds");
    REQUIRE(output.stepCount > 0 && output.stepCount <= VOX_PATTERN_MAX_STEPS,
            "Lead POD pattern bounded");
    REQUIRE(provider->generatePattern(instrument::InstrumentId("wrong"),
                "psy-arp", context, nullptr, output)
                == instrument::ContentStatus::notFound,
            "wrong-instrument Lead generation rejected");

    std::cout << "Lead tests passed (" << checks << ", compliance "
              << compliance.checks << ")\n";
    return EXIT_SUCCESS;
}
