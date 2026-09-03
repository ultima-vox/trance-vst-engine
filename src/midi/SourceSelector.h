#pragma once

namespace vstengine::midi {

// Canonical MIDI source mode. The integer values must stay in sync with the
// APVTS "midiMode" choice indices (0..3) and with the UI combo box orders
// (AUTO, PIANO ROLL, GENERATOR, BOTH).
enum class SourceMode : int {
    autoDetect = 0,
    pianoRoll,
    generator,
    both
};

// The ONLY place that maps per-block input observations to the
// "should the internal generator run" decision.
//
//   AUTO:       external (host) notes OR GUI keyboard notes win; the generator
//               only runs when neither is present.
//   PIANO ROLL: host/GUI input only; the generator never runs.
//   GENERATOR:  only internal generated material. (Host input is dropped by
//               the caller; the GUI keyboard intentionally keeps working.)
//   BOTH:       host/GUI input and generated material are combined.
//
// Kept free of JUCE so the full mode x input-presence matrix is unit-testable
// without a plugin instance. Note that "generated notes present" is NOT an
// input here: internally generated MIDI is never an input observation, so it
// can never suppress generator playback (regression: AUTO used to be silenced
// by its own generated notes via the virtual keyboard state).
[[nodiscard]] constexpr bool shouldRunGenerator(const SourceMode mode,
                                                const bool hasExternalNotes,
                                                const bool hasGuiNotes) noexcept
{
    switch (mode) {
        case SourceMode::autoDetect: return !(hasExternalNotes || hasGuiNotes);
        case SourceMode::pianoRoll:  return false;
        case SourceMode::generator:  return true;
        case SourceMode::both:       return true;
    }
    return false;
}

} // namespace vstengine::midi