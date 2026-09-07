#pragma once

// Kick engine "sound" parameter IDs (issue #11 PHASE 5), owned by the core
// module alongside the psy-bass sound IDs.
//
// Sound presets store exactly these (plus the psy-bass sound IDs); global
// MIDI/generator settings belong to Full presets only. The ID strings and
// order are part of the persisted-state compatibility contract and must not
// change. Sound-preset readers apply parameters per ID and skip entries the
// running build does not have, so v1 presets written before the kick engine
// (which contain no kick entries) keep loading unchanged.

namespace vstengine::core {

inline constexpr const char* kickSoundParameterIds[] = {
    "kickPitchStart", "kickPitchEnd", "kickPitchDecay", "kickPitchCurve",
    "kickBodyDecay", "kickTail", "kickClick", "kickClickTone", "kickDrive",
    "kickClip", "kickTransient", "kickSub", "kickTune", "kickPhase",
    "kickOutputLevel"
};
inline constexpr int numKickSoundParameterIds = 15;

// Global (non-sound) kick routing setting: the MIDI channel the synthesized
// kick listens on. Fixed multitimbral mapping per issue #11 §2.6 is
// Part 1 / CH 1 = Bass, Part 2 / CH 2 = Kick, hence the default of 2.
inline constexpr const char* kickMidiChannelParameterId = "kickMidiChannel";

} // namespace vstengine::core
