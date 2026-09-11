#pragma once
#include "instrument/InstrumentAbi.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vstengine::instrument {

using SlotId = std::uint64_t;
using InstrumentId = std::string;
using ParameterId = std::string;
inline constexpr SlotId invalidSlotId = 0;
inline constexpr SlotId initialSlotId(std::size_t creationOrdinal) noexcept
{
    return 0x5654450000000001ULL + creationOrdinal;
}

inline constexpr std::uint32_t contractVersion = VOX_INSTRUMENT_ABI_V1;
inline constexpr std::uint32_t descriptorSchemaVersion =
    VOX_INSTRUMENT_DESCRIPTOR_SCHEMA_V1;
inline constexpr std::uint32_t stateSchemaVersion =
    VOX_INSTRUMENT_STATE_SCHEMA_V1;
inline constexpr std::size_t maxSlots = 16;
inline constexpr std::size_t macrosPerSlot = 8;

struct ResourceBudget {
    std::uint32_t maxVoices {};
    std::uint32_t maxMidiEventsPerBlock {};
    std::uint32_t maxStateBytes {};
    std::uint32_t maxResourceBytes {};
    std::uint32_t maxLatencySamples {};
    std::uint32_t maxTailSamples {};
};

struct ParameterDescriptor {
    ParameterId id;
    std::string name;
    std::string unit;
    float minimum {};
    float maximum { 1.0f };
    float defaultValue {};
    bool automatable { true };
};

struct InstrumentDescriptor {
    InstrumentId id;
    std::string name;
    std::string vendor;
    std::uint32_t abiVersion { contractVersion };
    std::uint32_t descriptorVersion { descriptorSchemaVersion };
    std::uint32_t stateVersion { stateSchemaVersion };
    ResourceBudget budget;
    std::vector<ParameterDescriptor> parameters;
};

class ResourceResolver {
public:
    virtual ~ResourceResolver() = default;
    // Called during construction/state preparation only; never audio callback.
    virtual std::span<const std::byte> resolve(std::string_view resourceId) = 0;
};

struct PrepareSpec {
    double sampleRate {};
    std::uint32_t maximumBlockSize {};
    std::uint32_t outputChannels {};
};

struct ProcessBlock {
    std::span<float*> outputs;
    std::uint32_t sampleCount {};
    std::span<const VoxMidiEventV1> midi;
    double sampleRate {};
    double bpm {};
    double ppqPosition {};
    bool transportPlaying {};
};

class InstrumentInstance {
public:
    virtual ~InstrumentInstance() = default;
    virtual bool prepare(const PrepareSpec&) = 0;
    virtual void reset() noexcept = 0;
    virtual void setBypassed(bool) noexcept = 0;
    virtual void process(const ProcessBlock&) noexcept = 0;
    virtual bool setParameter(std::string_view id, float plainValue) noexcept = 0;
    virtual std::uint32_t latencySamples() const noexcept = 0;
    virtual std::uint32_t tailSamples() const noexcept = 0;
};

struct CreateContext {
    SlotId slotId {};
    ResourceResolver* resources {};
};

class InstrumentProvider {
public:
    virtual ~InstrumentProvider() = default;
    virtual std::span<const InstrumentDescriptor> descriptors() const noexcept = 0;
    // Host calls create only outside realtime processing.
    virtual std::unique_ptr<InstrumentInstance> create(
        std::string_view instrumentId, const CreateContext&) = 0;
};

enum class ResolutionStatus {
    resolved,
    missingModule,
    incompatibleAbi,
    incompatibleSchema,
    invalidDescriptor,
    budgetExceeded,
    constructionFailed
};

struct Resolution {
    ResolutionStatus status { ResolutionStatus::missingModule };
    const InstrumentDescriptor* descriptor {};
    InstrumentProvider* provider {};
    std::string diagnostic;
    explicit operator bool() const noexcept
    {
        return status == ResolutionStatus::resolved;
    }
};

} // namespace vstengine::instrument
