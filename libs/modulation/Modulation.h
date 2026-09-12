#pragma once

#include "instrument/InstrumentContract.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace vstengine::modulation {

inline constexpr std::size_t maxDestinations = 512;
inline constexpr std::size_t maxRoutes = 256;
inline constexpr std::size_t maxLfos = 2;
inline constexpr std::size_t maxMacros = instrument::macrosPerSlot;

using DestinationKey = std::uint64_t;

// Stable across registry order, process launches and platforms. Hash collisions
// are rejected while building the control-thread registry.
[[nodiscard]] constexpr DestinationKey destinationKey(
    std::string_view instrumentId, std::string_view parameterId) noexcept
{
    DestinationKey hash = 14695981039346656037ULL;
    const auto append = [&hash](std::string_view text) constexpr {
        for (const auto c : text) {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 1099511628211ULL;
        }
    };
    append(instrumentId);
    hash ^= 0xffu;
    hash *= 1099511628211ULL;
    append(parameterId);
    return hash;
}

struct Destination {
    DestinationKey key {};
    instrument::InstrumentId instrumentId;
    instrument::ParameterId parameterId;
    float minimum {};
    float maximum { 1.0f };
    float defaultValue {};
    float step {};
    instrument::ParameterType type { instrument::ParameterType::floating };
};

enum class RegistryStatus : std::uint8_t {
    ok,
    full,
    invalidDescriptor,
    duplicateParameter,
    hashCollision
};

// Built on control thread from module descriptors, then immutable during audio.
class DestinationRegistry final {
public:
    RegistryStatus add(const instrument::InstrumentDescriptor& descriptor);
    void clear() noexcept { count_ = 0; }

    [[nodiscard]] const Destination* find(DestinationKey key) const noexcept;
    [[nodiscard]] const Destination* find(std::string_view instrumentId,
                                          std::string_view parameterId) const noexcept;
    [[nodiscard]] std::uint16_t indexOf(DestinationKey key) const noexcept;
    [[nodiscard]] std::span<const Destination> destinations() const noexcept
    {
        return { destinations_.data(), count_ };
    }

private:
    std::array<Destination, maxDestinations> destinations_ {};
    std::uint16_t count_ {};
};

enum class SourceKind : std::uint8_t {
    macro,
    lfo,
    velocity,
    modWheel,
    aftertouch,
    envelope,
    stepMod,
    sampleAndHold,
    random
};

struct SourceId {
    SourceKind kind { SourceKind::macro };
    std::uint8_t index {};
};

enum class SourceTransform : std::uint8_t {
    direct,
    bipolar
};

struct Route {
    SourceId source;
    SourceTransform transform { SourceTransform::direct };
    DestinationKey destination {};
    float depth {};
};

struct SourceValues {
    std::array<float, maxMacros> macros {};
    std::array<float, maxLfos> lfos {};
    float envelope {};
    float stepMod {};
    float sampleAndHold {};
    float random {};
    float velocity {};
    float modWheel {};
    float aftertouch {};
};

struct DestinationValue {
    DestinationKey key {};
    std::uint16_t registryIndex {};
    float normalized {};
    float plain {};
};

enum class MatrixStatus : std::uint8_t {
    ok,
    notPrepared,
    invalidBudget,
    routeBudgetExceeded,
    invalidSource,
    invalidDestination,
    invalidDepth,
    duplicateRoute
};

// Prepared/configured off audio thread. One matrix belongs to one audio-thread
// runtime. evaluate() performs no allocation, locking or I/O.
class ModulationMatrix final {
public:
    MatrixStatus prepare(const DestinationRegistry& registry,
                         std::uint32_t declaredRouteBudget) noexcept;
    MatrixStatus addRoute(const Route& route) noexcept;
    bool removeRoute(std::size_t index) noexcept;
    void clearRoutes() noexcept { routeCount_ = 0; }

    [[nodiscard]] std::span<const Route> routes() const noexcept
    {
        return { routes_.data(), routeCount_ };
    }
    [[nodiscard]] std::uint32_t routeLimit() const noexcept { return routeLimit_; }

    // baseNormalized follows registry order. Returned values include only
    // routed destinations, always in stable registry order.
    [[nodiscard]] std::span<const DestinationValue> evaluate(
        const SourceValues& sources,
        std::span<const float> baseNormalized) noexcept;

private:
    static bool sourceValid(SourceId source) noexcept;
    void sortRoutes() noexcept;

    const DestinationRegistry* registry_ {};
    std::array<Route, maxRoutes> routes_ {};
    std::array<std::uint16_t, maxRoutes> destinationIndices_ {};
    std::array<float, maxDestinations> accumulated_ {};
    std::array<bool, maxDestinations> touched_ {};
    std::array<DestinationValue, maxDestinations> results_ {};
    std::uint32_t routeLimit_ {};
    std::uint16_t routeCount_ {};
    std::uint16_t resultCount_ {};
};

} // namespace vstengine::modulation
