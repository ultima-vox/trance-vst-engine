#pragma once
#include "instrument/InstrumentContract.h"
#include <memory>

namespace vstengine::instrument {

struct HostBudgetLimits {
    std::uint32_t maxVoicesPerSlot { 64 };
    std::uint32_t maxMidiEventsPerBlock { 2048 };
    std::uint32_t maxStateBytesPerSlot { 1024 * 1024 };
    std::uint32_t maxResourceBytesPerSlot { 64 * 1024 * 1024 };
    std::uint32_t maxLatencySamples { 192000 };
    std::uint32_t maxTailSamples { 1920000 };
    std::uint32_t maxPatternEventsPerSlot { 8192 };
    std::uint32_t maxModulationRoutesPerSlot { 256 };
    std::uint32_t maxScratchBytesPerSlot { 8 * 1024 * 1024 };
};

class InstrumentRegistry final {
public:
    explicit InstrumentRegistry(HostBudgetLimits limits = {}) : limits_(limits) {}

    bool registerProvider(std::unique_ptr<InstrumentProvider> provider,
                          std::string& diagnostic);
    [[nodiscard]] Resolution resolve(std::string_view instrumentId) const noexcept;
    [[nodiscard]] std::unique_ptr<InstrumentInstance> create(
        std::string_view instrumentId, const CreateContext&,
        Resolution& result) const;
    [[nodiscard]] std::span<const InstrumentDescriptor* const>
        descriptors() const noexcept { return descriptors_; }

private:
    struct RejectedModule {
        InstrumentId id;
        ResolutionStatus status;
        std::string diagnostic;
    };
    Resolution validate(InstrumentProvider&, const InstrumentDescriptor&) const;
    HostBudgetLimits limits_;
    std::vector<std::unique_ptr<InstrumentProvider>> providers_;
    std::vector<const InstrumentDescriptor*> descriptors_;
    std::vector<RejectedModule> rejected_;
};

} // namespace vstengine::instrument
