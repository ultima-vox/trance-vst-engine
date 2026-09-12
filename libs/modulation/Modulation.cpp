#include "modulation/Modulation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vstengine::modulation {
namespace {

constexpr std::uint16_t invalidIndex = std::numeric_limits<std::uint16_t>::max();

float sourceValue(const SourceValues& values, SourceId source) noexcept
{
    float value = 0.0f;
    switch (source.kind) {
    case SourceKind::macro: value = values.macros[source.index]; break;
    case SourceKind::lfo: value = values.lfos[source.index]; break;
    case SourceKind::envelope: value = values.envelope; break;
    case SourceKind::stepMod: value = values.stepMod; break;
    case SourceKind::sampleAndHold: value = values.sampleAndHold; break;
    case SourceKind::random: value = values.random; break;
    case SourceKind::velocity: value = values.velocity; break;
    case SourceKind::modWheel: value = values.modWheel; break;
    case SourceKind::aftertouch: value = values.aftertouch; break;
    }
    if (!std::isfinite(value)) return 0.0f;
    return source.kind == SourceKind::lfo
        ? std::clamp(value, -1.0f, 1.0f)
        : std::clamp(value, 0.0f, 1.0f);
}

float toPlain(const Destination& destination, float normalized) noexcept
{
    auto plain = destination.minimum
        + normalized * (destination.maximum - destination.minimum);
    const auto discrete = destination.type == instrument::ParameterType::integer
        || destination.type == instrument::ParameterType::choice;
    const auto quantum = destination.step > 0.0f
        ? destination.step : (discrete ? 1.0f : 0.0f);
    if (quantum > 0.0f)
        plain = destination.minimum
            + std::round((plain - destination.minimum) / quantum) * quantum;
    if (destination.type == instrument::ParameterType::boolean)
        plain = normalized >= 0.5f ? destination.maximum : destination.minimum;
    return std::clamp(plain, destination.minimum, destination.maximum);
}

} // namespace

RegistryStatus DestinationRegistry::add(
    const instrument::InstrumentDescriptor& descriptor)
{
    if (descriptor.id.empty()) return RegistryStatus::invalidDescriptor;

    std::size_t additions = 0;
    for (const auto& parameter : descriptor.parameters) {
        if (!parameter.modulatable) continue;
        if (parameter.id.empty() || !std::isfinite(parameter.minimum)
            || !std::isfinite(parameter.maximum)
            || !std::isfinite(parameter.defaultValue)
            || !std::isfinite(parameter.step)
            || parameter.minimum > parameter.maximum
            || parameter.defaultValue < parameter.minimum
            || parameter.defaultValue > parameter.maximum
            || parameter.step < 0.0f)
            return RegistryStatus::invalidDescriptor;

        ++additions;
        const auto key = destinationKey(descriptor.id, parameter.id);
        for (std::uint16_t i = 0; i < count_; ++i) {
            const auto& existing = destinations_[i];
            if (existing.key != key) continue;
            if (existing.instrumentId == descriptor.id
                && existing.parameterId == parameter.id)
                return RegistryStatus::duplicateParameter;
            return RegistryStatus::hashCollision;
        }
        for (const auto& other : descriptor.parameters) {
            if (&other == &parameter) break;
            if (other.modulatable && other.id == parameter.id)
                return RegistryStatus::duplicateParameter;
        }
    }
    if (additions > maxDestinations - count_) return RegistryStatus::full;

    for (const auto& parameter : descriptor.parameters) {
        if (!parameter.modulatable) continue;
        const auto key = destinationKey(descriptor.id, parameter.id);
        destinations_[count_++] = {
            key, descriptor.id, parameter.id, parameter.minimum,
            parameter.maximum, parameter.defaultValue, parameter.step,
            parameter.type
        };
    }
    return RegistryStatus::ok;
}

const Destination* DestinationRegistry::find(DestinationKey key) const noexcept
{
    const auto index = indexOf(key);
    return index == invalidIndex ? nullptr : &destinations_[index];
}

const Destination* DestinationRegistry::find(
    std::string_view instrumentId, std::string_view parameterId) const noexcept
{
    const auto* found = find(destinationKey(instrumentId, parameterId));
    if (found != nullptr && found->instrumentId == instrumentId
        && found->parameterId == parameterId)
        return found;
    return nullptr;
}

std::uint16_t DestinationRegistry::indexOf(DestinationKey key) const noexcept
{
    for (std::uint16_t i = 0; i < count_; ++i)
        if (destinations_[i].key == key) return i;
    return invalidIndex;
}

MatrixStatus ModulationMatrix::prepare(
    const DestinationRegistry& registry,
    std::uint32_t declaredRouteBudget) noexcept
{
    if (declaredRouteBudget > maxRoutes)
        return MatrixStatus::invalidBudget;
    registry_ = &registry;
    routeLimit_ = declaredRouteBudget;
    routeCount_ = 0;
    resultCount_ = 0;
    return MatrixStatus::ok;
}

bool ModulationMatrix::sourceValid(SourceId source) noexcept
{
    switch (source.kind) {
    case SourceKind::macro: return source.index < maxMacros;
    case SourceKind::lfo: return source.index < maxLfos;
    case SourceKind::velocity:
    case SourceKind::modWheel:
    case SourceKind::aftertouch:
    case SourceKind::envelope:
    case SourceKind::stepMod:
    case SourceKind::sampleAndHold:
    case SourceKind::random: return source.index == 0;
    }
    return false;
}

MatrixStatus ModulationMatrix::addRoute(const Route& route) noexcept
{
    if (registry_ == nullptr) return MatrixStatus::notPrepared;
    if (routeCount_ >= routeLimit_) return MatrixStatus::routeBudgetExceeded;
    if (!sourceValid(route.source)) return MatrixStatus::invalidSource;
    if (route.transform != SourceTransform::direct
        && route.transform != SourceTransform::bipolar)
        return MatrixStatus::invalidSource;
    if (!std::isfinite(route.depth) || route.depth < -1.0f || route.depth > 1.0f)
        return MatrixStatus::invalidDepth;
    const auto destinationIndex = registry_->indexOf(route.destination);
    if (destinationIndex == invalidIndex) return MatrixStatus::invalidDestination;
    for (std::uint16_t i = 0; i < routeCount_; ++i) {
        const auto& existing = routes_[i];
        if (existing.source.kind == route.source.kind
            && existing.source.index == route.source.index
            && existing.destination == route.destination)
            return MatrixStatus::duplicateRoute;
    }
    routes_[routeCount_] = route;
    destinationIndices_[routeCount_++] = destinationIndex;
    sortRoutes();
    return MatrixStatus::ok;
}

bool ModulationMatrix::removeRoute(std::size_t index) noexcept
{
    if (index >= routeCount_) return false;
    for (auto i = index + 1; i < routeCount_; ++i) {
        routes_[i - 1] = routes_[i];
        destinationIndices_[i - 1] = destinationIndices_[i];
    }
    --routeCount_;
    return true;
}

void ModulationMatrix::sortRoutes() noexcept
{
    for (std::uint16_t i = 1; i < routeCount_; ++i) {
        const auto route = routes_[i];
        const auto destinationIndex = destinationIndices_[i];
        auto position = i;
        while (position > 0) {
            const auto before = position - 1;
            const auto ordered = destinationIndices_[before] < destinationIndex
                || (destinationIndices_[before] == destinationIndex
                    && (routes_[before].source.kind < route.source.kind
                        || (routes_[before].source.kind == route.source.kind
                            && routes_[before].source.index <= route.source.index)));
            if (ordered) break;
            routes_[position] = routes_[before];
            destinationIndices_[position] = destinationIndices_[before];
            --position;
        }
        routes_[position] = route;
        destinationIndices_[position] = destinationIndex;
    }
}

std::span<const DestinationValue> ModulationMatrix::evaluate(
    const SourceValues& sources,
    std::span<const float> baseNormalized) noexcept
{
    resultCount_ = 0;
    if (registry_ == nullptr) return {};
    const auto destinations = registry_->destinations();
    if (baseNormalized.size() < destinations.size()) return {};

    std::fill_n(accumulated_.begin(), destinations.size(), 0.0f);
    std::fill_n(touched_.begin(), destinations.size(), false);
    for (std::uint16_t i = 0; i < routeCount_; ++i) {
        const auto& route = routes_[i];
        const auto destinationIndex = destinationIndices_[i];
        auto value = sourceValue(sources, route.source);
        if (route.transform == SourceTransform::bipolar
            && route.source.kind != SourceKind::lfo)
            value = value * 2.0f - 1.0f;
        accumulated_[destinationIndex] += value * route.depth;
        touched_[destinationIndex] = true;
    }

    for (std::uint16_t i = 0; i < destinations.size(); ++i) {
        if (!touched_[i]) continue;
        const auto normalized = std::clamp(
            std::isfinite(baseNormalized[i]) ? baseNormalized[i] + accumulated_[i]
                                             : 0.0f,
            0.0f, 1.0f);
        results_[resultCount_++] = {
            destinations[i].key, i, normalized,
            toPlain(destinations[i], normalized)
        };
    }
    return { results_.data(), resultCount_ };
}

} // namespace vstengine::modulation
