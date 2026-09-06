#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>
#include "sequence/Sequence.h"

namespace vstengine::midiexport {

struct Options {
    int channel { 1 };
    int rootNote { 36 };
    double ticksPerQuarter { 960.0 };
    // Seed for deterministic probability decisions during export. Export applies
    // the same splitmix32 RNG as realtime playback so a given seed reproduces
    // the same pattern in both paths.
    std::uint32_t seed { 0 };
};

// Deterministic MIDI representation of a Sequence. Used by the drag-to-host
// export so exported MIDI always matches the canonical model.
//
// Timing:
//   Step duration is derived from the canonical Sequence timing mode
//   (stepsPerQuarterNote), identical to realtime playback.
//
// Ratchet:
//   A step with ratchetCount = N emits N repeated sub-notes spread evenly
//   across the step. Each sub-note length is (stepDuration / N) * gateWidth,
//   so sub-notes never overlap and no note can get stuck.
//
// Slide:
//   A step with slideDuration > 0 glides from the previous audible note's
//   pitch into this note. Standard MIDI files cannot carry the continuous
//   internal portamento, so the closest deterministic representation is a
//   pitch-bend ramp (GM bend range: +/-2 semitones, RPN 0) from the previous
//   note's pitch to this note's pitch, ending exactly at the note-on.
//   The ramp covers slideDuration steps and is emitted in fixed
//   slideRampSegments steps so output is byte-for-byte reproducible.
//   The ramp start is clamped to tick 0 (no negative timestamps).
constexpr int slideRampSegments = 8;
constexpr double pitchBendRangeSemitones = 2.0;

// Builds the note track for a sequence (note on/off, ratchet sub-notes and
// slide pitch-bend ramps). Every note-on is matched with a note-off.
[[nodiscard]] juce::MidiMessageSequence buildSequenceTrack(
    const vstengine::sequence::Sequence& seq, const Options& options);

} // namespace vstengine::midiexport