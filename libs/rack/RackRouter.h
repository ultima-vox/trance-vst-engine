#pragma once
#include "rack/RackTypes.h"
#include <array>
#include <span>

namespace vstengine::rack {

class RackRouter final {
public:
    static constexpr std::size_t eventCapacity = 2048;
    struct DestinationBuffer {
        std::array<VoxMidiEventV1, eventCapacity> events {};
        std::size_t size {};
        std::uint64_t dropped {};
        void clear() noexcept { size = 0; }
        bool push(VoxMidiEventV1 event) noexcept;
        std::span<const VoxMidiEventV1> view() const noexcept
        {
            return { events.data(), size };
        }
    };

    void reset() noexcept;
    void forgetSlot(instrument::SlotId) noexcept;
    void route(std::span<const VoxMidiEventV1> input,
               std::span<const ProcessSlotControls> slots,
               std::array<DestinationBuffer, instrument::maxSlots>& output)
        noexcept;
    void routeAudition(std::span<const VoxMidiEventV1> input,
                       instrument::SlotId selectedSlot,
                       std::span<const ProcessSlotControls> slots,
                       std::array<DestinationBuffer, instrument::maxSlots>& output,
                       bool generated = false)
        noexcept;

private:
    struct Owner {
        instrument::SlotId slotId {};
        std::uint8_t transposedNote {};
    };
    struct Owners {
        std::array<Owner, instrument::maxSlots> values {};
        std::uint8_t size {};
    };
    std::array<std::array<Owners, 128>, 16> ownership_ {};
    std::array<Owner, 128> auditionOwnership_ {};
    std::array<Owner, 128> generatedOwnership_ {};
};

} // namespace vstengine::rack
