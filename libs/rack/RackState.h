#pragma once
#include "rack/RackTypes.h"
#include <juce_data_structures/juce_data_structures.h>

namespace vstengine::rack::state {
inline constexpr int schemaVersion = 3;
// Absolute host-side bound applies before module resolution. Missing modules
// cannot bypass state resource limits by carrying an unbounded opaque payload.
inline constexpr std::size_t maxModulePayloadBytes = 1024u * 1024u;
inline constexpr std::size_t maxPatternPayloadBytes = 64u * 1024u;
inline constexpr std::size_t maxModulationPayloadBytes = 64u * 1024u;
juce::ValueTree serialize(
    const std::array<PersistentSlotState, instrument::maxSlots>&);
bool deserialize(const juce::ValueTree&,
                 std::array<PersistentSlotState, instrument::maxSlots>&,
                 std::string& diagnostic);
} // namespace vstengine::rack::state
