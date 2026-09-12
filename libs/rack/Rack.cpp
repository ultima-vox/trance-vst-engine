#include "rack/Rack.h"
#include "rack/RackState.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace vstengine::rack {
namespace {
float macroToPlain(float normalized,
                   const instrument::ParameterDescriptor& parameter) noexcept
{
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    auto plain = parameter.minimum
        + normalized * (parameter.maximum - parameter.minimum);
    if (parameter.step > 0.0f)
        plain = parameter.minimum + std::round(
            (plain - parameter.minimum) / parameter.step) * parameter.step;
    return std::clamp(plain, parameter.minimum, parameter.maximum);
}
const instrument::ParameterDescriptor* findParameter(
    const instrument::InstrumentDescriptor& descriptor,
    std::string_view id) noexcept
{
    for (const auto& parameter : descriptor.parameters)
        if (parameter.id == id) return &parameter;
    return nullptr;
}
} // namespace

Rack::Rack(instrument::InstrumentRegistry& registry) : registry_(registry)
{
    for (std::size_t i = 0; i < state_.size(); ++i)
        state_[i].slotId = instrument::initialSlotId(i);
}

void Rack::beginExclusive() const noexcept
{
    mutationGate_.store(true, std::memory_order_release);
    while (activeProcessors_.load(std::memory_order_acquire) != 0)
        std::this_thread::yield();
}

void Rack::endExclusive() const noexcept
{
    mutationGate_.store(false, std::memory_order_release);
}

bool Rack::prepare(const instrument::PrepareSpec& spec)
{
    if (spec.sampleRate <= 0.0 || spec.maximumBlockSize == 0
        || spec.outputChannels == 0 || spec.outputChannels > 2)
        return false;
    spec_ = spec;
    for (auto& slot : runtime_) {
        for (std::size_t channel = 0; channel < spec.outputChannels; ++channel) {
            slot.audio[channel].assign(spec.maximumBlockSize, 0.0f);
            slot.pointers[channel] = slot.audio[channel].data();
        }
        if (slot.instance && !slot.instance->prepare(spec)) return false;
    }
    reset();
    return true;
}

void Rack::reset() noexcept
{
    router_.reset();
    for (auto& destination : routed_) {
        destination.size = 0;
        destination.dropped = 0;
    }
    for (auto& slot : runtime_)
        if (slot.instance) slot.instance->reset();
    for (auto& state : runtimeState_) state.ownedNotes = 0;
}

bool Rack::buildSlot(std::size_t index, const PersistentSlotState& state,
                     instrument::ResourceResolver* resources, SlotRuntime& target,
                     RuntimeSlotState& runtimeState, std::string& diagnostic)
{
    if (index >= state_.size() || state.schemaVersion != 2
        || state.slotId == instrument::invalidSlotId || !valid(state.routing)
        || !std::isfinite(state.level) || !std::isfinite(state.pan)
        || state.level < 0.0f || state.level > 2.0f
        || state.pan < -1.0f || state.pan > 1.0f) {
        diagnostic = "invalid rack slot state";
        return false;
    }
    if (state.instrumentId.empty()) {
        runtimeState.resolution = instrument::ResolutionStatus::missingModule;
        return true;
    }
    if (state.patternSchemaVersion > VOX_PATTERN_SCHEMA_V1) {
        runtimeState.resolution = instrument::ResolutionStatus::incompatibleSchema;
        diagnostic = "instrument pattern schema is newer than host";
        return true;
    }
    instrument::PersistentModuleState moduleState {
        state.instrumentStateVersion, state.slotId, state.instrumentId,
        state.resolvedProviderId, state.instrumentVersion, state.contentVersion,
        state.modulePayload
    };
    auto prepared = instrument::prepareModuleState(moduleState, registry_, resources);
    runtimeState.resolution = prepared.resolution.status;
    target.descriptor = prepared.resolution.descriptor;
    target.instance = std::move(prepared.instance);
    if (!target.instance) {
        diagnostic = prepared.resolution.diagnostic;
        return true; // Explicit unresolved slot is valid persistent state.
    }
    // Mapping identity persists independently from descriptor order. Populate
    // only previously-unassigned macros from descriptor preferences.
    if (spec_.sampleRate > 0.0 && !target.instance->prepare(spec_)) {
        target.instance.reset();
        target.descriptor = nullptr;
        runtimeState.resolution = instrument::ResolutionStatus::constructionFailed;
        diagnostic = "instrument prepare failed";
        return true;
    }
    for (std::size_t channel = 0; channel < spec_.outputChannels; ++channel) {
        target.audio[channel].assign(spec_.maximumBlockSize, 0.0f);
        target.pointers[channel] = target.audio[channel].data();
    }
    for (std::size_t macro = 0; macro < state.macros.size(); ++macro) {
        std::string_view parameterId = state.macroAssignments[macro];
        if (parameterId.empty())
            for (const auto& parameter : target.descriptor->parameters)
                if (parameter.preferredMacro == static_cast<std::int8_t>(macro)) {
                    parameterId = parameter.id;
                    break;
                }
        if (!parameterId.empty())
            if (const auto* parameter = findParameter(*target.descriptor, parameterId))
                target.instance->setParameter(parameterId,
                    macroToPlain(state.macros[macro], *parameter));
    }
    return true;
}

bool Rack::replaceState(
    const std::array<PersistentSlotState, instrument::maxSlots>& replacement,
    instrument::ResourceResolver* resources, std::string& diagnostic)
{
    std::array<SlotRuntime, instrument::maxSlots> preparedRuntime;
    std::array<RuntimeSlotState, instrument::maxSlots> preparedState;
    for (std::size_t i = 0; i < replacement.size(); ++i)
        if (!buildSlot(i, replacement[i], resources, preparedRuntime[i],
                       preparedState[i], diagnostic))
            return false;
    beginExclusive();
    state_ = replacement;
    runtime_ = std::move(preparedRuntime);
    runtimeState_ = preparedState;
    for (std::size_t i = 0; i < state_.size(); ++i)
        if (const auto* descriptor = runtime_[i].descriptor) {
            state_[i].resolvedProviderId = descriptor->providerId;
            state_[i].instrumentVersion = descriptor->instrumentVersion;
            state_[i].instrumentStateVersion = descriptor->stateVersion;
            state_[i].contentVersion = descriptor->contentVersion;
            for (const auto& parameter : descriptor->parameters)
                if (parameter.preferredMacro >= 0
                    && parameter.preferredMacro < static_cast<std::int8_t>(
                        instrument::macrosPerSlot)
                    && state_[i].macroAssignments[
                        static_cast<std::size_t>(parameter.preferredMacro)].empty())
                    state_[i].macroAssignments[
                        static_cast<std::size_t>(parameter.preferredMacro)] = parameter.id;
        }
    router_.reset();
    for (auto& destination : routed_) {
        destination.size = 0;
        destination.dropped = 0;
    }
    endExclusive();
    return true;
}

bool Rack::loadModule(std::size_t index, std::string_view instrumentId,
                      instrument::ResourceResolver* resources,
                      std::string& diagnostic)
{
    if (index >= state_.size()) { diagnostic = "slot index out of range"; return false; }
    auto replacement = state_[index];
    replacement.instrumentId = instrumentId;
    replacement.modulePayload.clear();
    replacement.patternSchemaVersion = VOX_PATTERN_SCHEMA_V1;
    replacement.patternPayload.clear();
    replacement.resolvedProviderId.clear();
    replacement.instrumentVersion = 0;
    replacement.instrumentStateVersion = instrument::stateSchemaVersion;
    replacement.contentVersion = 0;
    replacement.soundPreset.clear();
    replacement.patternPreset.clear();
    replacement.macroAssignments.fill({});
    replacement.macros.fill(0.0f);
    const auto resolution = registry_.resolve(instrumentId);
    if (resolution && resolution.descriptor)
        for (const auto& parameter : resolution.descriptor->parameters)
            if (parameter.preferredMacro >= 0
                && parameter.preferredMacro < static_cast<std::int8_t>(
                    instrument::macrosPerSlot)) {
                const auto macro = static_cast<std::size_t>(parameter.preferredMacro);
                replacement.macroAssignments[macro] = parameter.id;
                const auto range = parameter.maximum - parameter.minimum;
                replacement.macros[macro] = range > 0.0f
                    ? std::clamp((parameter.defaultValue - parameter.minimum) / range,
                                 0.0f, 1.0f)
                    : 0.0f;
            }
    SlotRuntime prepared;
    RuntimeSlotState preparedState;
    if (!buildSlot(index, replacement, resources, prepared, preparedState,
                   diagnostic)) return false;
    beginExclusive();
    if (runtime_[index].instance) runtime_[index].instance->reset();
    router_.forgetSlot(state_[index].slotId);
    routed_[index].size = 0;
    routed_[index].dropped = 0;
    state_[index] = std::move(replacement);
    if (prepared.descriptor) {
        state_[index].resolvedProviderId = prepared.descriptor->providerId;
        state_[index].instrumentVersion = prepared.descriptor->instrumentVersion;
        state_[index].instrumentStateVersion = prepared.descriptor->stateVersion;
        state_[index].contentVersion = prepared.descriptor->contentVersion;
        for (const auto& parameter : prepared.descriptor->parameters)
            if (parameter.preferredMacro >= 0
                && parameter.preferredMacro < static_cast<std::int8_t>(
                    instrument::macrosPerSlot))
                state_[index].macroAssignments[
                    static_cast<std::size_t>(parameter.preferredMacro)] = parameter.id;
    }
    runtime_[index] = std::move(prepared);
    runtimeState_[index] = preparedState;
    endExclusive();
    return true;
}

bool Rack::applySoundPreset(std::size_t index, std::string_view presetId,
                            instrument::ResourceResolver* resources,
                            std::string& diagnostic)
{
    if (index >= state_.size()) {
        diagnostic = "slot index out of range";
        return false;
    }
    auto replacement = state_[index];
    const auto resolution = registry_.resolve(replacement.instrumentId);
    if (!resolution || resolution.provider == nullptr) {
        diagnostic = resolution.diagnostic;
        return false;
    }
    const instrument::ContentDescriptor* content = nullptr;
    for (const auto& candidate : resolution.provider->contentDescriptors(
             replacement.instrumentId))
        if (candidate.kind == instrument::ContentKind::soundPreset
            && candidate.id == presetId) {
            content = &candidate;
            break;
        }
    if (content == nullptr) {
        diagnostic = "sound preset not found";
        return false;
    }
    SlotRuntime prepared;
    RuntimeSlotState preparedState;
    if (!buildSlot(index, replacement, resources, prepared, preparedState,
                   diagnostic)
        || !prepared.instance)
        return false;
    const auto status = resolution.provider->applySoundPreset(
        replacement.instrumentId, presetId, *prepared.instance);
    if (status != instrument::ContentStatus::ok) {
        diagnostic = "sound preset incompatible";
        return false;
    }
    replacement.modulePayload.resize(resolution.descriptor->budget.maxStateBytes);
    std::uint32_t written {};
    if (!prepared.instance->saveState(replacement.modulePayload, written)
        || written > replacement.modulePayload.size()) {
        diagnostic = "sound preset state exceeds budget";
        return false;
    }
    replacement.modulePayload.resize(written);
    replacement.soundPreset = std::string(presetId);
    for (std::size_t macro = 0; macro < replacement.macros.size(); ++macro)
        if ((content->macroValueMask & (1u << macro)) != 0)
            replacement.macros[macro] = std::clamp(
                content->macroValues[macro], 0.0f, 1.0f);

    beginExclusive();
    if (runtime_[index].instance) runtime_[index].instance->reset();
    router_.forgetSlot(replacement.slotId);
    state_[index] = std::move(replacement);
    runtime_[index] = std::move(prepared);
    runtimeState_[index] = preparedState;
    routed_[index].clear();
    endExclusive();
    diagnostic.clear();
    return true;
}

bool Rack::updatePatternState(std::size_t index, std::string presetId,
                              std::uint32_t patternSchema,
                              std::vector<std::byte> payload)
{
    if (index >= state_.size() || patternSchema == 0
        || payload.size() > state::maxPatternPayloadBytes)
        return false;
    beginExclusive();
    state_[index].patternPreset = std::move(presetId);
    state_[index].patternSchemaVersion = patternSchema;
    state_[index].patternPayload = std::move(payload);
    endExclusive();
    return true;
}

void Rack::setMacro(std::size_t slot, std::size_t macro, float value) noexcept
{
    if (slot >= state_.size() || macro >= instrument::macrosPerSlot
        || !std::isfinite(value)) return;
    value = std::clamp(value, 0.0f, 1.0f);
    state_[slot].macros[macro] = value;
    auto& target = runtime_[slot];
    if (target.instance && target.descriptor) {
        const auto& mapped = state_[slot].macroAssignments[macro];
        if (!mapped.empty()) target.instance->setParameter(mapped, value);
    }
}

void Rack::updateControls(std::size_t slot, Routing routing, bool enabled,
                          bool mute, bool solo, bool locked, float level,
                          float pan) noexcept
{
    if (slot >= state_.size() || !valid(routing) || !std::isfinite(level)
        || !std::isfinite(pan)) return;
    state_[slot].routing = routing;
    state_[slot].enabled = enabled;
    state_[slot].mute = mute;
    state_[slot].solo = solo;
    state_[slot].locked = locked;
    state_[slot].level = std::clamp(level, 0.0f, 2.0f);
    state_[slot].pan = std::clamp(pan, -1.0f, 1.0f);
}

void Rack::process(std::span<float*> outputs, std::uint32_t sampleCount,
                   std::span<const VoxMidiEventV1> midi, double bpm,
                   double ppq, bool playing,
                   std::span<const VoxMidiEventV1> audition,
                   instrument::SlotId selectedSlot,
                   std::span<const ProcessSlotControls> controls,
                   std::span<const RackRouter::GeneratedSlotEvents> generated) noexcept
{
    if (sampleCount > spec_.maximumBlockSize || outputs.empty()
        || outputs.size() > 2 || mutationGate_.load(std::memory_order_acquire))
        return;
    activeProcessors_.fetch_add(1, std::memory_order_acq_rel);
    if (mutationGate_.load(std::memory_order_acquire)) {
        activeProcessors_.fetch_sub(1, std::memory_order_release);
        return;
    }
    if (controls.size() != state_.size()) {
        for (std::size_t i = 0; i < state_.size(); ++i)
            defaultControls_[i] = processControls(state_[i]);
        controls = defaultControls_;
    }
    router_.route(midi, controls, routed_);
    router_.routeAudition(audition, selectedSlot, controls, routed_);
    router_.routeGenerated(generated, controls, routed_);
    for (auto& destination : routed_)
        destination.sortBySampleOffset();
    const bool anySolo = std::any_of(controls.begin(), controls.end(),
                                     [](const auto& slot) { return slot.solo; });
    for (std::size_t i = 0; i < state_.size(); ++i) {
        auto& slot = runtime_[i];
        runtimeState_[i].droppedMidiEvents = routed_[i].dropped;
        if (!slot.instance) continue;
        for (std::size_t channel = 0; channel < outputs.size(); ++channel)
            std::fill_n(slot.audio[channel].data(), sampleCount, 0.0f);
        for (std::size_t macro = 0; macro < instrument::macrosPerSlot; ++macro) {
            const auto& mapped = state_[i].macroAssignments[macro];
            if (!mapped.empty())
                if (const auto* parameter = findParameter(*slot.descriptor, mapped))
                    slot.instance->setParameter(mapped,
                        macroToPlain(controls[i].macros[macro], *parameter));
        }
        slot.instance->setBypassed(!controls[i].enabled || controls[i].mute
                                  || (anySolo && !controls[i].solo));
        slot.instance->process({ { slot.pointers.data(), outputs.size() },
            sampleCount, routed_[i].view(), spec_.sampleRate, bpm, ppq, playing });
        const float left = controls[i].level * (controls[i].pan <= 0.0f
            ? 1.0f : 1.0f - controls[i].pan);
        const float right = controls[i].level * (controls[i].pan >= 0.0f
            ? 1.0f : 1.0f + controls[i].pan);
        for (std::uint32_t sample = 0; sample < sampleCount; ++sample) {
            outputs[0][sample] += slot.audio[0][sample] * left;
            if (outputs.size() > 1)
                outputs[1][sample] += slot.audio[1][sample] * right;
        }
    }
    activeProcessors_.fetch_sub(1, std::memory_order_release);
}

const instrument::InstrumentDescriptor* Rack::descriptor(std::size_t index) const noexcept
{
    return index < runtime_.size() ? runtime_[index].descriptor : nullptr;
}

std::array<PersistentSlotState, instrument::maxSlots> Rack::snapshotState(
    std::span<const ProcessSlotControls> controls) const
{
    beginExclusive();
    auto snapshot = state_;
    for (std::size_t index = 0; index < snapshot.size(); ++index) {
        if (controls.size() == snapshot.size()) {
            snapshot[index].routing = controls[index].routing;
            snapshot[index].enabled = controls[index].enabled;
            snapshot[index].mute = controls[index].mute;
            snapshot[index].solo = controls[index].solo;
            snapshot[index].locked = controls[index].locked;
            snapshot[index].level = controls[index].level;
            snapshot[index].pan = controls[index].pan;
            snapshot[index].macros = controls[index].macros;
        }
        const auto& live = runtime_[index];
        if (!live.instance || !live.descriptor) continue;
        auto& payload = snapshot[index].modulePayload;
        payload.resize(live.descriptor->budget.maxStateBytes);
        std::uint32_t written = 0;
        if (!live.instance->saveState(payload, written)
            || written > payload.size()) {
            payload = state_[index].modulePayload;
            continue;
        }
        payload.resize(written);
    }
    endExclusive();
    return snapshot;
}

std::uint32_t Rack::latencySamples() const noexcept
{
    std::uint32_t result = 0;
    for (const auto& slot : runtime_)
        if (slot.instance) result = std::max(result, slot.instance->latencySamples());
    return result;
}
std::uint32_t Rack::tailSamples() const noexcept
{
    std::uint32_t result = 0;
    for (const auto& slot : runtime_)
        if (slot.instance) result = std::max(result, slot.instance->tailSamples());
    return result;
}
} // namespace vstengine::rack
