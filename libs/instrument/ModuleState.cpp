#include "instrument/ModuleState.h"
#include "instrument/InstrumentRegistry.h"

namespace vstengine::instrument {
PreparedModuleState prepareModuleState(const PersistentModuleState& state,
                                       const InstrumentRegistry& registry,
                                       ResourceResolver* resources)
{
    PreparedModuleState prepared;
    prepared.persistent = state;
    if (state.schemaVersion == 0 || state.schemaVersion > stateSchemaVersion
        || state.slotId == 0 || state.instrumentId.empty()) {
        prepared.resolution.status = ResolutionStatus::incompatibleSchema;
        prepared.resolution.diagnostic = "invalid persistent module state";
        return prepared;
    }
    prepared.resolution = registry.resolve(state.instrumentId);
    if (!prepared.resolution)
        return prepared;
    if (state.schemaVersion > prepared.resolution.descriptor->stateVersion) {
        prepared.resolution.status = ResolutionStatus::incompatibleSchema;
        prepared.resolution.diagnostic = "instrument state schema is newer than module";
        prepared.resolution.descriptor = nullptr;
        prepared.resolution.provider = nullptr;
        return prepared;
    }
    if (state.payload.size()
        > prepared.resolution.descriptor->budget.maxStateBytes) {
        prepared.resolution.status = ResolutionStatus::budgetExceeded;
        prepared.resolution.diagnostic = "instrument state exceeds module budget";
        prepared.resolution.descriptor = nullptr;
        prepared.resolution.provider = nullptr;
        return prepared;
    }
    prepared.instance = registry.create(state.instrumentId,
        { state.slotId, resources }, prepared.resolution);
    return prepared;
}
} // namespace vstengine::instrument
