#pragma once
#include "instrument/InstrumentRegistry.h"
#include "rack/RackRouter.h"
#include <array>
#include <atomic>

namespace vstengine::rack {

class Rack final {
public:
    explicit Rack(instrument::InstrumentRegistry&);
    bool prepare(const instrument::PrepareSpec&);
    void reset() noexcept;
    bool replaceState(
        const std::array<PersistentSlotState, instrument::maxSlots>&,
        instrument::ResourceResolver*, std::string& diagnostic);
    bool loadModule(std::size_t slotIndex, std::string_view instrumentId,
                    instrument::ResourceResolver*, std::string& diagnostic);
    bool applySoundPreset(std::size_t slotIndex, std::string_view presetId,
                          instrument::ResourceResolver*, std::string& diagnostic);
    bool updatePatternState(std::size_t slotIndex, std::string presetId,
                            std::uint32_t schemaVersion,
                            std::vector<std::byte> payload);
    void process(std::span<float*> outputs, std::uint32_t sampleCount,
                 std::span<const VoxMidiEventV1> midi, double bpm,
                 double ppqPosition, bool playing,
                 std::span<const VoxMidiEventV1> audition = {},
                 instrument::SlotId selectedSlot = instrument::invalidSlotId,
                 std::span<const ProcessSlotControls> controls = {},
                 std::span<const RackRouter::GeneratedSlotEvents> generated = {}) noexcept;
    void setMacro(std::size_t slotIndex, std::size_t macroIndex,
                  float value) noexcept;
    void updateControls(std::size_t slotIndex, Routing, bool enabled,
                        bool mute, bool solo, bool locked, float level,
                        float pan) noexcept;
    [[nodiscard]] const auto& state() const noexcept { return state_; }
    [[nodiscard]] std::array<PersistentSlotState, instrument::maxSlots>
        snapshotState(std::span<const ProcessSlotControls> controls = {}) const;
    [[nodiscard]] const auto& runtimeState() const noexcept { return runtimeState_; }
    [[nodiscard]] const instrument::InstrumentDescriptor* descriptor(
        std::size_t slotIndex) const noexcept;
    [[nodiscard]] std::uint32_t latencySamples() const noexcept;
    [[nodiscard]] std::uint32_t tailSamples() const noexcept;

private:
    struct SlotRuntime {
        std::unique_ptr<instrument::InstrumentInstance> instance;
        const instrument::InstrumentDescriptor* descriptor {};
        std::array<std::vector<float>, 2> audio;
        std::array<float*, 2> pointers {};
    };
    bool buildSlot(std::size_t, const PersistentSlotState&,
                   instrument::ResourceResolver*, SlotRuntime&,
                   RuntimeSlotState&, std::string& diagnostic);
    void beginExclusive() const noexcept;
    void endExclusive() const noexcept;

    instrument::InstrumentRegistry& registry_;
    instrument::PrepareSpec spec_;
    std::array<PersistentSlotState, instrument::maxSlots> state_ {};
    std::array<RuntimeSlotState, instrument::maxSlots> runtimeState_ {};
    std::array<SlotRuntime, instrument::maxSlots> runtime_ {};
    std::array<RackRouter::DestinationBuffer, instrument::maxSlots> routed_ {};
    std::array<ProcessSlotControls, instrument::maxSlots> defaultControls_ {};
    RackRouter router_;
    mutable std::atomic<bool> mutationGate_ { false };
    mutable std::atomic<std::uint32_t> activeProcessors_ { 0 };
};

} // namespace vstengine::rack
