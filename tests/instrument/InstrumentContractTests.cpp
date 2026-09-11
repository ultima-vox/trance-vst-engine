#include "instrument/HostParameterSchema.h"
#include "instrument/InstrumentRegistry.h"
#include "instrument/ModuleState.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
int run = 0;
int passed = 0;
#define REQUIRE(condition, message)                                      \
    do {                                                                  \
        ++run;                                                            \
        if (!(condition)) {                                               \
            std::cerr << "FAILED: " << message << " line " << __LINE__   \
                      << '\n';                                            \
            std::exit(EXIT_FAILURE);                                      \
        }                                                                 \
        ++passed;                                                         \
    } while (false)

class Instance final : public vstengine::instrument::InstrumentInstance {
public:
    bool prepare(const vstengine::instrument::PrepareSpec& next) override
    {
        spec = next;
        prepared = next.sampleRate > 0.0 && next.maximumBlockSize > 0
                && next.outputChannels > 0;
        return prepared;
    }
    void reset() noexcept override { phase = 0.0; }
    void suspend() noexcept override { suspended = true; }
    void resume() noexcept override { suspended = false; }
    void setBypassed(bool value) noexcept override { bypassed = value; }
    void process(const vstengine::instrument::ProcessBlock& block) noexcept override
    {
        ++processCalls;
        if (!prepared || bypassed || suspended
            || block.sampleCount > spec.maximumBlockSize)
            return;
        for (auto* channel : block.outputs)
            for (std::uint32_t sample = 0; sample < block.sampleCount; ++sample) {
                channel[sample] += static_cast<float>(std::sin(phase)) * gain;
                phase += 0.01;
            }
    }
    bool setParameter(std::string_view id, float value) noexcept override
    {
        if (id != "gain" || !std::isfinite(value)) return false;
        gain = value;
        return true;
    }
    bool loadState(std::uint32_t schema, std::span<const std::byte> payload) noexcept override
    {
        if (schema != vstengine::instrument::stateSchemaVersion) return false;
        if (payload.empty()) return true;
        if (payload.size() != sizeof(gain)) return false;
        std::memcpy(&gain, payload.data(), sizeof(gain));
        return std::isfinite(gain);
    }
    bool saveState(std::span<std::byte> destination,
                   std::uint32_t& written) const noexcept override
    {
        written = sizeof(gain);
        if (destination.size() < written) return false;
        std::memcpy(destination.data(), &gain, sizeof(gain));
        return true;
    }
    std::uint32_t latencySamples() const noexcept override { return 0; }
    std::uint32_t tailSamples() const noexcept override { return 256; }
    vstengine::instrument::PrepareSpec spec;
    bool prepared {};
    bool bypassed {};
    bool suspended {};
    int processCalls {};
    double phase {};
    float gain { 0.1f };
};

class Provider final : public vstengine::instrument::InstrumentProvider {
public:
    explicit Provider(std::uint32_t abi = vstengine::instrument::contractVersion,
                      std::uint32_t voices = 1)
    {
        descriptor.id = "com.ultimavox.reference";
        descriptor.providerId = "com.ultimavox.test-provider";
        descriptor.name = "Reference Instrument";
        descriptor.vendor = "Ultima Vox";
        descriptor.abiVersion = abi;
        descriptor.budget = { voices, 256, 4096, 0, 0, 256 };
        descriptor.parameters.push_back({ "gain", "Gain", "", 0.0f, 1.0f,
            0.1f, 0.0001f, vstengine::instrument::ParameterType::floating, "Output", {},
            true, true, 0 });
    }
    std::span<const vstengine::instrument::InstrumentDescriptor>
        descriptors() const noexcept override { return { &descriptor, 1 }; }
    std::unique_ptr<vstengine::instrument::InstrumentInstance> create(
        std::string_view id,
        const vstengine::instrument::CreateContext& context) override
    {
        ++createCalls;
        lastSlot = context.slotId;
        return id == descriptor.id ? std::make_unique<Instance>() : nullptr;
    }
    vstengine::instrument::InstrumentDescriptor descriptor;
    int createCalls {};
    vstengine::instrument::SlotId lastSlot {};
};
} // namespace

int main()
{
    using namespace vstengine::instrument;
    REQUIRE(std::is_standard_layout_v<VoxInstrumentApiV1>,
            "external ABI stays POD-compatible");
    REQUIRE(sizeof(VoxMidiEventV1) == 8, "MIDI ABI layout stable");
    REQUIRE(initialSlotId(0) != invalidSlotId
                && initialSlotId(0) != initialSlotId(1),
            "initial SlotIds are stable and unique");

    std::array<std::string, maxSlots * macrosPerSlot> ids;
    std::size_t idIndex = 0;
    for (std::size_t slot = 0; slot < maxSlots; ++slot)
        for (std::size_t macro = 0; macro < macrosPerSlot; ++macro)
            ids[idIndex++] = hostparams::macroId(slot, macro);
    REQUIRE(ids.front() == "slot01Macro01", "first host macro ID stable");
    REQUIRE(ids.back() == "slot16Macro08", "last host macro ID stable");
    for (std::size_t i = 0; i < ids.size(); ++i)
        for (std::size_t j = i + 1; j < ids.size(); ++j)
            REQUIRE(ids[i] != ids[j], "host macro IDs unique");
    REQUIRE(hostparams::macroName(0, 0) == "Slot 1 Cutoff",
            "Cubase macro label stable");

    InstrumentRegistry registry;
    auto provider = std::make_unique<Provider>();
    auto* providerView = provider.get();
    std::string diagnostic;
    REQUIRE(registry.registerProvider(std::move(provider), diagnostic),
            "compatible provider registers");
    REQUIRE(registry.resolve("com.ultimavox.reference"),
            "registered instrument resolves");
    REQUIRE(registry.resolve("com.ultimavox.missing").status
                == ResolutionStatus::missingModule,
            "missing module stays unresolved");

    Resolution created;
    auto instance = registry.create("com.ultimavox.reference", { 42, nullptr },
                                    created);
    REQUIRE(instance != nullptr && created, "factory creates through registry");
    REQUIRE(providerView->createCalls == 1 && providerView->lastSlot == 42,
            "factory receives stable SlotId");
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const std::uint32_t blockSize : { 1u, 64u, 511u, 2048u }) {
            REQUIRE(instance->prepare({ rate, blockSize, 2 }),
                    "lifecycle prepare accepts supported host shape");
            std::vector<float> left(blockSize), right(blockSize);
            std::array<float*, 2> outputs { left.data(), right.data() };
            instance->reset();
            instance->process({ outputs, blockSize, {}, rate, 145.0, 0.0, true });
            REQUIRE(std::isfinite(left.back()) && std::isfinite(right.back()),
                    "lifecycle render finite");
        }
    REQUIRE(providerView->createCalls == 1,
            "audio processing never constructs module");
    instance->setBypassed(true);
    std::array<float, 8> bypassSamples {};
    std::array<float*, 1> bypassOutput { bypassSamples.data() };
    instance->process({ bypassOutput, 8, {}, 48000.0, 145.0, 0.0, true });
    REQUIRE(bypassSamples[7] == 0.0f, "instrument bypass is explicit");
    instance->setBypassed(false);
    instance->suspend();
    instance->process({ bypassOutput, 8, {}, 48000.0, 145.0, 0.0, true });
    REQUIRE(bypassSamples[7] == 0.0f, "suspend is explicit and bounded");
    instance->resume();

    auto badAbi = std::make_unique<Provider>(contractVersion + 1);
    badAbi->descriptor.id = "com.ultimavox.badabi";
    REQUIRE(!registry.registerProvider(std::move(badAbi), diagnostic),
            "incompatible ABI rejected");
    REQUIRE(registry.resolve("com.ultimavox.badabi").status
                == ResolutionStatus::incompatibleAbi,
            "incompatible ABI remains explicit unresolved state");

    auto overBudget = std::make_unique<Provider>(contractVersion, 1000);
    overBudget->descriptor.id = "com.ultimavox.overbudget";
    REQUIRE(!registry.registerProvider(std::move(overBudget), diagnostic),
            "budget violation rejected");
    REQUIRE(registry.resolve("com.ultimavox.overbudget").status
                == ResolutionStatus::budgetExceeded,
            "budget failure remains diagnosable");

    const PersistentModuleState missing {
        stateSchemaVersion, 77, "com.ultimavox.not-installed",
        {}, 0, 0,
        { std::byte { 1 }, std::byte { 2 } }
    };
    auto prepared = prepareModuleState(missing, registry, nullptr);
    REQUIRE(!prepared.instance
                && prepared.resolution.status == ResolutionStatus::missingModule,
            "transaction preparation preserves unresolved module");
    REQUIRE(prepared.persistent.payload == missing.payload,
            "unresolved payload preserved without silent fallback");

    std::cout << "Instrument contract tests passed (" << passed << "/" << run
              << ")\n";
    return EXIT_SUCCESS;
}
