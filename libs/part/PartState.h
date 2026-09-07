#pragma once
#include "part/Part.h"
#include <juce_data_structures/juce_data_structures.h>

namespace vstengine::part {

// Versioned per-Part state serialization (issue #11 PHASE 7).
//
// Each Part's mix state (mute/solo/lock/level/pan) and its sequence blob are
// written to a child ValueTree so the full multitimbral arrangement survives
// Cubase project restore. The on-disk format is versioned; load dispatches on
// the version and rejects unknown/future versions cleanly.

static constexpr int currentPartStateVersion = 1;

// Writes one Part's state (mix params + base64 sequence blob) to a PARAM
// child of the given tree. The sequence is encoded like the shell's project
// state (Sequence::serialize -> base64) so it round-trips identically.
void writePartState(juce::ValueTree& tree, const Part& part, int index);

// Reads one Part's state from a PARAM child. Returns true on success (version
// matches and structural checks pass), false otherwise (live Part untouched).
bool readPartState(const juce::ValueTree& tree, Part& part, int index);

// Convenience: write/read a whole PartArray.
void writePartArray(juce::ValueTree& tree, const PartArray& parts);
bool readPartArray(const juce::ValueTree& tree, Part& part, int index);

} // namespace vstengine::part