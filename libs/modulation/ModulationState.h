#pragma once

#include "modulation/Modulation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace vstengine::modulation {

inline constexpr std::uint16_t modulationStateSchemaVersion = 1;
inline constexpr std::size_t maxPersistentIdBytes = 64;

struct PersistentRoute {
    SourceId source;
    SourceTransform transform { SourceTransform::direct };
    float depth {};
    std::uint8_t instrumentIdLength {};
    std::uint8_t parameterIdLength {};
    std::array<char, maxPersistentIdBytes> instrumentId {};
    std::array<char, maxPersistentIdBytes> parameterId {};
};

struct PersistentState {
    std::uint16_t schemaVersion { modulationStateSchemaVersion };
    std::uint16_t routeCount {};
    std::array<PersistentRoute, maxRoutes> routes {};
};

static_assert(std::is_trivially_copyable_v<PersistentRoute>);
static_assert(std::is_trivially_copyable_v<PersistentState>);

enum class StateStatus : std::uint8_t {
    ok,
    destinationMissing,
    idTooLong,
    routeBudgetExceeded,
    destinationMismatch,
    outputTooSmall,
    corrupt,
    incompatibleSchema,
    invalidRoute
};

// Control-thread helpers. Runtime phases and source values are intentionally
// excluded. restoreState commits only after every route validates.
StateStatus captureState(const ModulationMatrix& matrix,
                         const DestinationRegistry& registry,
                         PersistentState& destination) noexcept;
StateStatus restoreState(const PersistentState& state,
                         const DestinationRegistry& registry,
                         std::uint32_t declaredRouteBudget,
                         ModulationMatrix& destination) noexcept;

[[nodiscard]] std::size_t serializedSize(const PersistentState& state) noexcept;
StateStatus serializeState(const PersistentState& state,
                           std::span<std::byte> destination,
                           std::size_t& bytesWritten) noexcept;
StateStatus deserializeState(std::span<const std::byte> bytes,
                             PersistentState& destination) noexcept;

} // namespace vstengine::modulation
