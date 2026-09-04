#pragma once
#include <cmath>
#include <cstdint>

namespace vstengine::sync {

// Pure musical-grid math used by realtime host-PPQ synchronization.
//
// The plugin's sequence sits on a fixed grid of stepsPerQuarterNote() steps
// per quarter note (the canonical Sequence timing mode; see Sequence.h). This
// module resolves a host quarter-note position against that grid. It has no
// JUCE / plugin / audio-buffer dependencies, so the alignment rules used by
// transport start, seek and cycle-loop wrap are directly unit-testable and by
// construction identical in every code path that consumes them.

struct StepGridPosition {
    // Absolute step number on the musical grid. Can be negative when the host
    // timeline extends before bar 1.
    std::int64_t absoluteStep {};
    // Canonical sequence step index in [0, seqSize).
    int stepIndex {};
    // Elapsed fraction of the containing step in [0, 1).
    double stepFraction {};
};

// Resolves a host quarter-note position against the step grid. The sequence
// step index depends ONLY on the musical position and the canonical grid; it
// is independent of tempo, which is what keeps musical alignment intact across
// BPM changes.
[[nodiscard]] inline StepGridPosition resolveStepGrid(const double ppqQuarters,
                                                      const int seqSize,
                                                      const double stepsPerQuarter) noexcept
{
    StepGridPosition out;
    const auto absolute = ppqQuarters * stepsPerQuarter;
    out.absoluteStep = static_cast<std::int64_t>(std::floor(absolute));
    out.stepFraction = absolute - static_cast<double>(out.absoluteStep);
    const auto wrapped = std::fmod(
        std::fmod(static_cast<double>(out.absoluteStep),
                  static_cast<double>(seqSize))
            + static_cast<double>(seqSize),
        static_cast<double>(seqSize));
    out.stepIndex = static_cast<int>(wrapped);
    return out;
}

// The absolute step boundary that starts at or immediately after the given
// PPQ position. A block starting exactly on a boundary (within epsilon)
// returns that boundary, so transport starts / seeks that land on a step
// boundary fire the step at sample offset 0 instead of skipping it.
[[nodiscard]] inline std::int64_t nextStepBoundary(const double ppqQuarters,
                                                   const double stepsPerQuarter,
                                                   const double epsilon = 1e-6) noexcept
{
    const auto absolute = ppqQuarters * stepsPerQuarter;
    const auto floored = std::floor(absolute);
    return (absolute - floored) <= epsilon
        ? static_cast<std::int64_t>(floored)
        : static_cast<std::int64_t>(floored) + 1;
}

// Converts a quarter-note duration into samples at the given tempo.
[[nodiscard]] inline double quartersToSamples(const double quarters,
                                              const double bpm,
                                              const double sampleRate) noexcept
{
    return quarters * (60.0 / bpm) * sampleRate;
}

} // namespace vstengine::sync