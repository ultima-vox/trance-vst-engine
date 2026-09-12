#include "modulation/ModulationState.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <string_view>

namespace vstengine::modulation {
namespace {

constexpr std::array<std::byte, 4> magic {
    std::byte { 'V' }, std::byte { 'M' }, std::byte { 'O' }, std::byte { 'D' }
};
constexpr std::size_t headerBytes = 8;
constexpr std::size_t routeHeaderBytes = 12;

void writeU16(std::span<std::byte> bytes, std::size_t& offset,
              std::uint16_t value) noexcept
{
    bytes[offset++] = static_cast<std::byte>(value & 0xffu);
    bytes[offset++] = static_cast<std::byte>((value >> 8u) & 0xffu);
}

void writeU32(std::span<std::byte> bytes, std::size_t& offset,
              std::uint32_t value) noexcept
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes[offset++] = static_cast<std::byte>((value >> shift) & 0xffu);
}

bool readU16(std::span<const std::byte> bytes, std::size_t& offset,
             std::uint16_t& value) noexcept
{
    if (offset + 2 > bytes.size()) return false;
    value = std::to_integer<std::uint16_t>(bytes[offset])
        | (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8u);
    offset += 2;
    return true;
}

bool readU32(std::span<const std::byte> bytes, std::size_t& offset,
             std::uint32_t& value) noexcept
{
    if (offset + 4 > bytes.size()) return false;
    value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        value |= std::to_integer<std::uint32_t>(bytes[offset++]) << shift;
    return true;
}

std::string_view text(const std::array<char, maxPersistentIdBytes>& bytes,
                      std::uint8_t length) noexcept
{
    return { bytes.data(), length };
}

} // namespace

StateStatus captureState(const ModulationMatrix& matrix,
                         const DestinationRegistry& registry,
                         PersistentState& destination) noexcept
{
    PersistentState candidate;
    const auto routes = matrix.routes();
    if (routes.size() > maxRoutes) return StateStatus::routeBudgetExceeded;
    for (const auto& route : routes) {
        const auto* found = registry.find(route.destination);
        if (found == nullptr) return StateStatus::destinationMissing;
        if (found->instrumentId.size() > maxPersistentIdBytes
            || found->parameterId.size() > maxPersistentIdBytes)
            return StateStatus::idTooLong;
        auto& stored = candidate.routes[candidate.routeCount++];
        stored.source = route.source;
        stored.transform = route.transform;
        stored.depth = route.depth;
        stored.instrumentIdLength =
            static_cast<std::uint8_t>(found->instrumentId.size());
        stored.parameterIdLength =
            static_cast<std::uint8_t>(found->parameterId.size());
        std::memcpy(stored.instrumentId.data(), found->instrumentId.data(),
                    found->instrumentId.size());
        std::memcpy(stored.parameterId.data(), found->parameterId.data(),
                    found->parameterId.size());
    }
    destination = candidate;
    return StateStatus::ok;
}

StateStatus restoreState(const PersistentState& state,
                         const DestinationRegistry& registry,
                         std::uint32_t declaredRouteBudget,
                         ModulationMatrix& destination) noexcept
{
    if (state.schemaVersion != modulationStateSchemaVersion)
        return StateStatus::incompatibleSchema;
    if (state.routeCount > maxRoutes || state.routeCount > declaredRouteBudget)
        return StateStatus::routeBudgetExceeded;

    ModulationMatrix candidate;
    if (candidate.prepare(registry, declaredRouteBudget) != MatrixStatus::ok)
        return StateStatus::routeBudgetExceeded;
    for (std::uint16_t i = 0; i < state.routeCount; ++i) {
        const auto& stored = state.routes[i];
        if (stored.instrumentIdLength > maxPersistentIdBytes
            || stored.parameterIdLength > maxPersistentIdBytes)
            return StateStatus::corrupt;
        const auto instrumentId = text(stored.instrumentId,
                                       stored.instrumentIdLength);
        const auto parameterId = text(stored.parameterId,
                                      stored.parameterIdLength);
        const auto* found = registry.find(instrumentId, parameterId);
        if (found == nullptr) return StateStatus::destinationMissing;
        const Route route {
            stored.source, stored.transform, found->key, stored.depth
        };
        if (candidate.addRoute(route) != MatrixStatus::ok)
            return StateStatus::invalidRoute;
    }
    destination = candidate;
    return StateStatus::ok;
}

std::size_t serializedSize(const PersistentState& state) noexcept
{
    if (state.routeCount > maxRoutes) return 0;
    auto size = headerBytes;
    for (std::uint16_t i = 0; i < state.routeCount; ++i) {
        const auto& route = state.routes[i];
        if (route.instrumentIdLength > maxPersistentIdBytes
            || route.parameterIdLength > maxPersistentIdBytes)
            return 0;
        size += routeHeaderBytes + route.instrumentIdLength
            + route.parameterIdLength;
    }
    return size;
}

StateStatus serializeState(const PersistentState& state,
                           std::span<std::byte> destination,
                           std::size_t& bytesWritten) noexcept
{
    bytesWritten = 0;
    if (state.schemaVersion != modulationStateSchemaVersion)
        return StateStatus::incompatibleSchema;
    const auto required = serializedSize(state);
    if (required == 0) return StateStatus::corrupt;
    if (destination.size() < required) return StateStatus::outputTooSmall;

    std::size_t offset = 0;
    for (const auto byte : magic) destination[offset++] = byte;
    writeU16(destination, offset, state.schemaVersion);
    writeU16(destination, offset, state.routeCount);
    for (std::uint16_t i = 0; i < state.routeCount; ++i) {
        const auto& route = state.routes[i];
        destination[offset++] = static_cast<std::byte>(route.source.kind);
        destination[offset++] = static_cast<std::byte>(route.source.index);
        destination[offset++] = static_cast<std::byte>(route.transform);
        destination[offset++] = std::byte {};
        writeU32(destination, offset, std::bit_cast<std::uint32_t>(route.depth));
        destination[offset++] = static_cast<std::byte>(route.instrumentIdLength);
        destination[offset++] = static_cast<std::byte>(route.parameterIdLength);
        writeU16(destination, offset, 0);
        std::memcpy(destination.data() + offset, route.instrumentId.data(),
                    route.instrumentIdLength);
        offset += route.instrumentIdLength;
        std::memcpy(destination.data() + offset, route.parameterId.data(),
                    route.parameterIdLength);
        offset += route.parameterIdLength;
    }
    bytesWritten = offset;
    return StateStatus::ok;
}

StateStatus deserializeState(std::span<const std::byte> bytes,
                             PersistentState& destination) noexcept
{
    if (bytes.size() < headerBytes) return StateStatus::corrupt;
    for (std::size_t i = 0; i < magic.size(); ++i)
        if (bytes[i] != magic[i]) return StateStatus::corrupt;
    std::size_t offset = magic.size();
    PersistentState candidate;
    if (!readU16(bytes, offset, candidate.schemaVersion)
        || candidate.schemaVersion != modulationStateSchemaVersion)
        return StateStatus::incompatibleSchema;
    if (!readU16(bytes, offset, candidate.routeCount)
        || candidate.routeCount > maxRoutes)
        return StateStatus::corrupt;

    for (std::uint16_t i = 0; i < candidate.routeCount; ++i) {
        if (offset + routeHeaderBytes > bytes.size()) return StateStatus::corrupt;
        auto& route = candidate.routes[i];
        route.source.kind = static_cast<SourceKind>(
            std::to_integer<std::uint8_t>(bytes[offset++]));
        route.source.index = std::to_integer<std::uint8_t>(bytes[offset++]);
        route.transform = static_cast<SourceTransform>(
            std::to_integer<std::uint8_t>(bytes[offset++]));
        if (bytes[offset++] != std::byte {}) return StateStatus::corrupt;
        std::uint32_t depthBits {};
        if (!readU32(bytes, offset, depthBits)) return StateStatus::corrupt;
        route.depth = std::bit_cast<float>(depthBits);
        route.instrumentIdLength = std::to_integer<std::uint8_t>(bytes[offset++]);
        route.parameterIdLength = std::to_integer<std::uint8_t>(bytes[offset++]);
        std::uint16_t reserved {};
        if (!readU16(bytes, offset, reserved)
            || reserved != 0
            || route.instrumentIdLength > maxPersistentIdBytes
            || route.parameterIdLength > maxPersistentIdBytes
            || offset + route.instrumentIdLength + route.parameterIdLength
                > bytes.size())
            return StateStatus::corrupt;
        std::memcpy(route.instrumentId.data(), bytes.data() + offset,
                    route.instrumentIdLength);
        offset += route.instrumentIdLength;
        std::memcpy(route.parameterId.data(), bytes.data() + offset,
                    route.parameterIdLength);
        offset += route.parameterIdLength;
        if (!std::isfinite(route.depth)) return StateStatus::invalidRoute;
    }
    if (offset != bytes.size()) return StateStatus::corrupt;
    destination = candidate;
    return StateStatus::ok;
}

} // namespace vstengine::modulation
