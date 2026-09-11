#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

// External-module-ready C ABI. No STL, JUCE, exceptions, ownership-bearing
// pointers or compiler-specific classes may cross this boundary.
extern "C" {

enum : std::uint32_t {
    VOX_INSTRUMENT_ABI_V1 = 1,
    VOX_INSTRUMENT_DESCRIPTOR_SCHEMA_V1 = 1,
    VOX_INSTRUMENT_STATE_SCHEMA_V1 = 1,
    VOX_INSTRUMENT_MAX_ID_BYTES = 64,
    VOX_INSTRUMENT_MAX_NAME_BYTES = 96
};

struct VoxInstrumentIdV1 {
    char bytes[VOX_INSTRUMENT_MAX_ID_BYTES];
};

struct VoxInstrumentBudgetV1 {
    std::uint32_t maxVoices;
    std::uint32_t maxMidiEventsPerBlock;
    std::uint32_t maxStateBytes;
    std::uint32_t maxResourceBytes;
    std::uint32_t maxLatencySamples;
    std::uint32_t maxTailSamples;
};

struct VoxParameterDescriptorV1 {
    const char* parameterId;
    const char* displayName;
    const char* unit;
    float minimum;
    float maximum;
    float defaultValue;
    std::uint32_t flags;
};

struct VoxInstrumentDescriptorV1 {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    std::uint32_t descriptorSchemaVersion;
    std::uint32_t stateSchemaVersion;
    VoxInstrumentIdV1 instrumentId;
    const char* displayName;
    const char* vendor;
    VoxInstrumentBudgetV1 budget;
    const VoxParameterDescriptorV1* parameters;
    std::uint32_t parameterCount;
};

struct VoxMidiEventV1 {
    std::uint32_t sampleOffset;
    std::uint8_t size;
    std::uint8_t data[3];
};

struct VoxProcessContextV1 {
    std::uint32_t structSize;
    float** outputs;
    std::uint32_t outputChannels;
    std::uint32_t sampleCount;
    const VoxMidiEventV1* midiEvents;
    std::uint32_t midiEventCount;
    double sampleRate;
    double bpm;
    double ppqPosition;
    std::uint32_t transportFlags;
};

using VoxInstrumentHandleV1 = void*;
using VoxCreateInstrumentV1 = VoxInstrumentHandleV1 (*)(
    const VoxInstrumentIdV1*, const void* hostServices) noexcept;
using VoxDestroyInstrumentV1 = void (*)(VoxInstrumentHandleV1) noexcept;
using VoxPrepareInstrumentV1 = bool (*)(VoxInstrumentHandleV1, double,
                                        std::uint32_t) noexcept;
using VoxResetInstrumentV1 = void (*)(VoxInstrumentHandleV1) noexcept;
using VoxSetInstrumentBypassedV1 = void (*)(VoxInstrumentHandleV1,
                                            bool) noexcept;
using VoxProcessInstrumentV1 = void (*)(VoxInstrumentHandleV1,
                                        const VoxProcessContextV1*) noexcept;
using VoxSetParameterV1 = bool (*)(VoxInstrumentHandleV1, const char*,
                                   float) noexcept;

struct VoxInstrumentApiV1 {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    const VoxInstrumentDescriptorV1* (*descriptorAt)(std::uint32_t) noexcept;
    std::uint32_t (*descriptorCount)() noexcept;
    VoxCreateInstrumentV1 create;
    VoxDestroyInstrumentV1 destroy;
    VoxPrepareInstrumentV1 prepare;
    VoxResetInstrumentV1 reset;
    VoxSetInstrumentBypassedV1 setBypassed;
    VoxProcessInstrumentV1 process;
    VoxSetParameterV1 setParameter;
};

using VoxGetInstrumentApiV1 = const VoxInstrumentApiV1* (*)() noexcept;

} // extern "C"

static_assert(sizeof(VoxMidiEventV1) == 8);
static_assert(std::is_standard_layout_v<VoxInstrumentDescriptorV1>);
static_assert(std::is_trivially_copyable_v<VoxProcessContextV1>);
