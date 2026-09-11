#include "instrument/InstrumentRegistry.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace vstengine::instrument {
namespace {
bool validId(std::string_view id)
{
    if (id.empty() || id.size() >= VOX_INSTRUMENT_MAX_ID_BYTES)
        return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '.' || c == '-' || c == '_';
    });
}
bool exceeds(const ResourceBudget& budget, const HostBudgetLimits& limits)
{
    return budget.maxVoices > limits.maxVoicesPerSlot
        || budget.maxMidiEventsPerBlock > limits.maxMidiEventsPerBlock
        || budget.maxStateBytes > limits.maxStateBytesPerSlot
        || budget.maxResourceBytes > limits.maxResourceBytesPerSlot
        || budget.maxLatencySamples > limits.maxLatencySamples
        || budget.maxTailSamples > limits.maxTailSamples
        || budget.maxPatternEvents > limits.maxPatternEventsPerSlot
        || budget.maxModulationRoutes > limits.maxModulationRoutesPerSlot
        || budget.scratchBytes > limits.maxScratchBytesPerSlot;
}
} // namespace

Resolution InstrumentRegistry::validate(InstrumentProvider& provider,
                                        const InstrumentDescriptor& descriptor) const
{
    Resolution result { ResolutionStatus::resolved, &descriptor, &provider, {} };
    if (descriptor.abiVersion != contractVersion) {
        result.status = ResolutionStatus::incompatibleAbi;
        result.diagnostic = "instrument ABI version mismatch";
    } else if (descriptor.descriptorVersion != descriptorSchemaVersion
               || descriptor.stateVersion == 0) {
        result.status = ResolutionStatus::incompatibleSchema;
        result.diagnostic = "instrument schema version mismatch";
    } else if (!validId(descriptor.id) || !validId(descriptor.providerId)
               || descriptor.name.empty()
               || descriptor.vendor.empty() || descriptor.budget.maxVoices == 0
               || descriptor.budget.maxMidiEventsPerBlock == 0
               || descriptor.instrumentVersion == 0
               || descriptor.minimumHostVersion > hostContractVersion
               || descriptor.contentVersion == 0
               || descriptor.supportedPresetSchemaVersions.empty()) {
        result.status = ResolutionStatus::invalidDescriptor;
        result.diagnostic = "invalid instrument descriptor";
    } else if (exceeds(descriptor.budget, limits_)) {
        result.status = ResolutionStatus::budgetExceeded;
        result.diagnostic = "instrument resource budget exceeds host limit";
    } else {
        std::unordered_set<std::string_view> ids;
        for (const auto& parameter : descriptor.parameters) {
            if (!validId(parameter.id) || parameter.name.empty()
                || !std::isfinite(parameter.minimum)
                || !std::isfinite(parameter.maximum)
                || !std::isfinite(parameter.defaultValue)
                || parameter.minimum >= parameter.maximum || parameter.step < 0.0f
                || parameter.defaultValue < parameter.minimum
                || parameter.defaultValue > parameter.maximum
                || parameter.preferredMacro >= static_cast<std::int8_t>(macrosPerSlot)
                || !ids.insert(parameter.id).second) {
                result.status = ResolutionStatus::invalidDescriptor;
                result.diagnostic = "invalid instrument parameter descriptor";
                break;
            }
        }
    }
    return result;
}

bool InstrumentRegistry::registerProvider(
    std::unique_ptr<InstrumentProvider> provider, std::string& diagnostic)
{
    if (provider == nullptr) {
        diagnostic = "null instrument provider";
        return false;
    }
    const auto offered = provider->descriptors();
    if (offered.empty()) {
        diagnostic = "instrument provider has no descriptors";
        return false;
    }
    for (const auto& descriptor : offered) {
        const auto validated = validate(*provider, descriptor);
        if (!validated) {
            if (validId(descriptor.id))
                rejected_.push_back(
                    { descriptor.id, validated.status, validated.diagnostic });
            diagnostic = validated.diagnostic;
            return false;
        }
        if (resolve(descriptor.id).status != ResolutionStatus::missingModule) {
            diagnostic = "duplicate instrument ID";
            return false;
        }
    }
    auto* accepted = provider.get();
    providers_.push_back(std::move(provider));
    for (const auto& descriptor : accepted->descriptors())
        descriptors_.push_back(&descriptor);
    return true;
}

Resolution InstrumentRegistry::resolve(std::string_view instrumentId) const noexcept
{
    for (const auto& provider : providers_)
        for (const auto& descriptor : provider->descriptors())
            if (descriptor.id == instrumentId)
                return { ResolutionStatus::resolved, &descriptor,
                         provider.get(), {} };
    for (const auto& rejected : rejected_)
        if (rejected.id == instrumentId)
            return { rejected.status, nullptr, nullptr, rejected.diagnostic };
    return { ResolutionStatus::missingModule, nullptr, nullptr,
             "instrument module is missing" };
}

std::unique_ptr<InstrumentInstance> InstrumentRegistry::create(
    std::string_view instrumentId, const CreateContext& context,
    Resolution& result) const
{
    result = resolve(instrumentId);
    if (!result)
        return {};
    std::unique_ptr<InstrumentInstance> instance;
    try {
        instance = result.provider->create(instrumentId, context);
    } catch (...) {
        result.status = ResolutionStatus::constructionFailed;
        result.diagnostic = "instrument construction threw an exception";
        return {};
    }
    if (instance == nullptr) {
        result.status = ResolutionStatus::constructionFailed;
        result.diagnostic = "instrument construction failed";
    }
    return instance;
}
} // namespace vstengine::instrument
