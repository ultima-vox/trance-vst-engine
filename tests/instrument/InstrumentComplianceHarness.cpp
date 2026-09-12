#include "instrument/InstrumentComplianceHarness.h"
#include "sequence/PatternAdapter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <unordered_set>
#include <utility>

namespace vstengine::tests {
namespace {

using instrument::InstrumentDescriptor;
using instrument::InstrumentInstance;

void check(InstrumentComplianceReport& report, bool condition,
           std::string message)
{
    ++report.checks;
    if (!condition) report.failures.push_back(std::move(message));
}

bool validStableId(std::string_view id) noexcept
{
    if (id.empty() || id.size() >= VOX_INSTRUMENT_MAX_ID_BYTES) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char value) {
        return (value >= 'a' && value <= 'z')
            || (value >= '0' && value <= '9') || value == '.'
            || value == '-' || value == '_';
    });
}

double energy(std::span<const float> audio, bool& finite) noexcept
{
    double result = 0.0;
    for (const float sample : audio) {
        if (!std::isfinite(sample)) {
            finite = false;
            return std::numeric_limits<double>::infinity();
        }
        result += static_cast<double>(sample) * sample;
    }
    return result;
}

struct RenderResult {
    std::vector<float> left;
    std::vector<float> right;
    bool finite { true };
    double totalEnergy {};
};

RenderResult render(InstrumentInstance& instance, double sampleRate,
                    std::uint32_t blockSize,
                    std::span<const VoxMidiEventV1> midi)
{
    RenderResult result;
    result.left.assign(blockSize, 0.0f);
    result.right.assign(blockSize, 0.0f);
    std::array<float*, 2> outputs { result.left.data(), result.right.data() };
    instance.process({ outputs, blockSize, midi, sampleRate, 145.0, 0.0, true });
    result.totalEnergy = energy(result.left, result.finite);
    result.totalEnergy += energy(result.right, result.finite);
    return result;
}

bool sameRender(const RenderResult& left, const RenderResult& right) noexcept
{
    return left.left == right.left && left.right == right.right;
}

const InstrumentDescriptor* findDescriptor(
    instrument::InstrumentProvider& provider, std::string_view instrumentId,
    std::size_t& matches) noexcept
{
    const InstrumentDescriptor* found = nullptr;
    matches = 0;
    for (const auto& descriptor : provider.descriptors())
        if (descriptor.id == instrumentId) {
            found = &descriptor;
            ++matches;
        }
    return found;
}

void validateDescriptor(InstrumentComplianceReport& report,
                        const InstrumentDescriptor& descriptor)
{
    check(report, validStableId(descriptor.id), "invalid InstrumentId");
    check(report, validStableId(descriptor.providerId), "invalid providerId");
    check(report, !descriptor.name.empty(), "missing display name");
    check(report, !descriptor.vendor.empty(), "missing vendor");
    check(report, descriptor.abiVersion == instrument::contractVersion,
          "instrument ABI version mismatch");
    check(report,
          descriptor.descriptorVersion == instrument::descriptorSchemaVersion,
          "descriptor schema version mismatch");
    check(report, descriptor.stateVersion > 0, "invalid state schema version");
    check(report, descriptor.instrumentVersion > 0,
          "invalid instrument version");
    check(report, descriptor.contentVersion > 0, "invalid content version");
    check(report,
          descriptor.minimumHostVersion <= instrument::hostContractVersion,
          "minimum host version unsupported");
    check(report, !descriptor.supportedPresetSchemaVersions.empty(),
          "missing supported preset schemas");
    check(report, descriptor.budget.maxVoices > 0, "zero voice budget");
    check(report, descriptor.budget.maxMidiEventsPerBlock > 0,
          "zero MIDI-event budget");
    check(report, descriptor.budget.maxStateBytes > 0, "zero state budget");

    std::unordered_set<std::string_view> parameterIds;
    for (const auto& parameter : descriptor.parameters) {
        check(report, validStableId(parameter.id),
              "invalid instrument ParameterId");
        check(report, !parameter.name.empty(), "missing parameter name");
        check(report,
              std::isfinite(parameter.minimum)
                  && std::isfinite(parameter.maximum)
                  && std::isfinite(parameter.defaultValue)
                  && std::isfinite(parameter.step),
              "non-finite parameter descriptor");
        check(report,
              parameter.minimum < parameter.maximum
                  && parameter.defaultValue >= parameter.minimum
                  && parameter.defaultValue <= parameter.maximum
                  && parameter.step >= 0.0f,
              "invalid parameter range/default/step");
        check(report,
              parameter.preferredMacro >= -1
                  && parameter.preferredMacro
                      < static_cast<std::int8_t>(instrument::macrosPerSlot),
              "preferred macro outside host bank");
        check(report, parameterIds.insert(parameter.id).second,
              "duplicate instrument ParameterId");
    }
}

} // namespace

InstrumentComplianceReport runInstrumentCompliance(
    instrument::InstrumentProvider& provider, std::string_view instrumentId,
    const InstrumentComplianceOptions& options)
{
    InstrumentComplianceReport report;
    std::size_t descriptorMatches = 0;
    const auto* descriptor = findDescriptor(provider, instrumentId,
                                            descriptorMatches);
    check(report, descriptorMatches == 1,
          "provider must expose InstrumentId exactly once");
    if (descriptor == nullptr) return report;
    validateDescriptor(report, *descriptor);

    if ((descriptor->capabilities
         & instrument::Capability::patternGenerator) != 0) {
        const auto content = provider.contentDescriptors(instrumentId);
        const auto profile = std::find_if(content.begin(), content.end(),
            [](const auto& item) {
                return item.kind == instrument::ContentKind::generatorProfile;
            });
        check(report, profile != content.end(),
              "patternGenerator capability has no generator profile");
        if (profile != content.end()) {
            VoxGenerationContextV1 context {};
            context.structSize = sizeof(context);
            context.globalSeed = 0x13579bdu;
            context.slotId = instrument::initialSlotId(7);
            std::snprintf(context.instrumentId.bytes,
                          sizeof(context.instrumentId.bytes), "%.*s",
                          static_cast<int>(instrumentId.size()),
                          instrumentId.data());
            VoxPatternV1 first { sizeof(VoxPatternV1) };
            VoxPatternV1 second { sizeof(VoxPatternV1) };
            check(report, provider.generatePattern(instrumentId, profile->id,
                    context, nullptr, first) == instrument::ContentStatus::ok
                    && sequence::validPattern(first),
                  "advertised generator profile failed");
            check(report, provider.generatePattern(instrumentId, profile->id,
                    context, nullptr, second) == instrument::ContentStatus::ok,
                  "generator repeat failed");
            std::vector<std::byte> firstBytes;
            std::vector<std::byte> secondBytes;
            check(report, sequence::encodePattern(first, firstBytes)
                    && sequence::encodePattern(second, secondBytes)
                    && firstBytes == secondBytes,
                  "generator is not deterministic");
        }
    }

    auto create = [&]() {
        return provider.create(instrumentId,
            { instrument::initialSlotId(7), nullptr });
    };
    auto instance = create();
    check(report, instance != nullptr, "factory failed for advertised instrument");
    if (!instance) return report;
    check(report,
          provider.create("com.ultimavox.compliance.unknown",
                          { instrument::initialSlotId(8), nullptr }) == nullptr,
          "factory created unadvertised instrument");

    const std::array sampleRates { 44100.0, 48000.0, 96000.0 };
    const std::array<std::uint32_t, 5> blockSizes { 64, 128, 256, 512, 1024 };
    const VoxMidiEventV1 noteOn {
        0, 3, { 0x90, options.testNote, options.testVelocity }
    };
    const std::array noteEvents { noteOn };

    for (const double sampleRate : sampleRates) {
        for (const auto blockSize : blockSizes) {
            check(report, instance->prepare({ sampleRate, blockSize, 2 }),
                  "prepare rejected required sample-rate/block-size pair");
            instance->reset();
            const auto first = render(*instance, sampleRate, blockSize,
                                      noteEvents);
            check(report, first.finite, "render produced NaN/Inf");
            if (options.requireAudibleNote)
                check(report, first.totalEnergy > options.audibleEnergy,
                      "note-capable instrument rendered silence");
            instance->reset();
            const auto second = render(*instance, sampleRate, blockSize,
                                       noteEvents);
            check(report, second.finite && sameRender(first, second),
                  "reset render is not deterministic");
            check(report,
                  instance->latencySamples()
                      <= descriptor->budget.maxLatencySamples,
                  "reported latency exceeds descriptor budget");
            check(report,
                  instance->tailSamples() <= descriptor->budget.maxTailSamples,
                  "reported tail exceeds descriptor budget");
            if (descriptor->tailPolicy == instrument::TailPolicy::none)
                check(report, instance->tailSamples() == 0,
                      "tail-free instrument reports a tail");
        }
    }

    constexpr double stateRate = 48000.0;
    constexpr std::uint32_t stateBlock = 256;
    check(report, instance->prepare({ stateRate, stateBlock, 2 }),
          "prepare failed before state test");
    for (const auto& parameter : descriptor->parameters)
        check(report,
              instance->setParameter(parameter.id, parameter.defaultValue),
              "descriptor parameter rejected its default value");
    if (!descriptor->parameters.empty()) {
        const auto& parameter = descriptor->parameters.front();
        const float probe = parameter.minimum
            + (parameter.maximum - parameter.minimum) * 0.731f;
        check(report, instance->setParameter(parameter.id, probe),
              "descriptor parameter rejected in-range value");
    }
    check(report,
          !instance->setParameter("compliance.unknown-parameter", 0.5f),
          "instance accepted unknown ParameterId");

    std::vector<std::byte> state(descriptor->budget.maxStateBytes);
    std::uint32_t written = 0;
    check(report, instance->saveState(state, written), "state save failed");
    check(report, written <= state.size(), "state exceeded declared budget");
    state.resize(std::min<std::size_t>(written, state.size()));
    auto restored = create();
    check(report, restored != nullptr, "factory failed for state restore");
    if (restored) {
        check(report, restored->prepare({ stateRate, stateBlock, 2 }),
              "restored instance prepare failed");
        check(report, restored->loadState(descriptor->stateVersion, state),
              "state load failed");
        std::vector<std::byte> roundTrip(descriptor->budget.maxStateBytes);
        std::uint32_t roundTripWritten = 0;
        check(report, restored->saveState(roundTrip, roundTripWritten),
              "round-trip state save failed");
        roundTrip.resize(std::min<std::size_t>(roundTripWritten,
                                               roundTrip.size()));
        check(report, roundTrip == state,
              "state save-load-save is not canonical");
    }

    instance->reset();
    instance->setBypassed(true);
    const auto bypassed = render(*instance, stateRate, stateBlock, noteEvents);
    check(report, bypassed.finite, "bypassed render produced NaN/Inf");
    if (descriptor->tailPolicy != instrument::TailPolicy::preserveOnBypass)
        check(report, bypassed.totalEnergy <= options.silenceEnergy,
              "bypassed instrument rendered new note energy");
    instance->setBypassed(false);
    instance->suspend();
    const auto suspended = render(*instance, stateRate, stateBlock, noteEvents);
    check(report, suspended.finite, "suspended render produced NaN/Inf");
    check(report, suspended.totalEnergy <= options.silenceEnergy,
          "suspended instrument rendered audio");
    instance->resume();

    constexpr std::uint32_t maximumTestEventBudget = 8192;
    check(report,
          descriptor->budget.maxMidiEventsPerBlock <= maximumTestEventBudget,
          "MIDI-event budget exceeds compliance harness safety bound");
    const auto eventCount = std::min(descriptor->budget.maxMidiEventsPerBlock,
                                     maximumTestEventBudget);
    std::vector<VoxMidiEventV1> saturatedEvents(eventCount);
    for (std::uint32_t index = 0; index < eventCount; ++index)
        saturatedEvents[index] = {
            static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(index) * stateBlock) / eventCount), 3,
            { 0xb0, 1, static_cast<std::uint8_t>(index % 128) }
        };
    instance->reset();
    const auto saturatedA = render(*instance, stateRate, stateBlock,
                                   saturatedEvents);
    instance->reset();
    const auto saturatedB = render(*instance, stateRate, stateBlock,
                                   saturatedEvents);
    check(report, saturatedA.finite && saturatedB.finite,
          "declared event-budget render produced NaN/Inf");
    check(report, sameRender(saturatedA, saturatedB),
          "event-budget saturation behavior is not deterministic");

    return report;
}

} // namespace vstengine::tests
