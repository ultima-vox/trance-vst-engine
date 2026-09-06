#pragma once

// Shared low-level identifiers owned by the core module.
//
// These are the psy-bass "sound" parameter IDs. Sound presets store exactly
// these; global MIDI/generator settings (midiMode, midiChannel, rootNote,
// rngSeed) and the sequence belong to Full presets only.
//
// They are shared by the plugin shell (APVTS layout), the preset module
// (factory preset tables) and UI wiring, so they live below every one of
// those modules. The ID strings and order are part of the persisted-state
// compatibility contract and must not change.

namespace vstengine::core {

inline constexpr const char* soundParameterIds[] = {
    "drive", "release", "ampAttack", "ampDecay", "ampSustain",
    "filterCutoff", "filterResonance", "filterDrive", "keyTracking",
    "pitchEnvAmount", "pitchEnvTime", "pitchEnvCurve", "outputLevel"
};
inline constexpr int numSoundParameterIds = 13;

} // namespace vstengine::core
