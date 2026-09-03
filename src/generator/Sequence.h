#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

namespace vstengine::generator {

enum class TimingMode : uint8_t {
    sixteenth,
    eighth,
    thirtySecond,
    triplet
};

// Canonical timing helper. This is the ONLY place that maps a TimingMode to a
// step duration; realtime playback and MIDI export must both derive their step
// size from this function so playback and export can never disagree.
//
// triplet mode = 1/8 triplets: three steps per quarter note.
[[nodiscard]] constexpr double stepsPerQuarterNote(TimingMode mode) noexcept
{
    switch (mode) {
        case TimingMode::eighth:       return 2.0;
        case TimingMode::thirtySecond: return 8.0;
        case TimingMode::triplet:      return 3.0;
        case TimingMode::sixteenth:    return 4.0;
    }
    return 4.0;
}

// Step duration expressed in quarter notes (multiply by samples/ticks per
// quarter to obtain real durations).
[[nodiscard]] constexpr double stepDurationInQuarterNotes(TimingMode mode) noexcept
{
    return 1.0 / stepsPerQuarterNote(mode);
}

[[nodiscard]] constexpr bool isValidTimingMode(uint8_t raw) noexcept
{
    return raw <= static_cast<uint8_t>(TimingMode::triplet);
}

// Deterministic probability state used by both realtime playback and MIDI
// export. The rule is: advance the RNG exactly once per sequence step,
// regardless of gate state, then evaluate probability from that step's roll.
// This ensures realtime + export produce identical pass/skip decisions for the
// same seed + sequence.
struct ProbabilityState {
    std::uint32_t state { 0 };
};

// Advances the RNG state once and returns true if the step should play.
// Call this for EVERY step (gated or not) to keep state in sync.
[[nodiscard]] inline bool shouldPlayStep(ProbabilityState& ps, float probability) noexcept
{
    // splitmix32: deterministic, allocation-free, no global state.
    ps.state += 0x9E3779B9u;
    auto z = ps.state;
    z = (z ^ (z >> 16)) * 0x21F0AAADu;
    z = (z ^ (z >> 15)) * 0x735A2D97u;
    const auto roll = static_cast<float>((z ^ (z >> 15)) >> 8)
                      * (1.0f / 16777216.0f);
    return probability >= 1.0f || roll < probability;
}

struct Step {
    bool gate { false };
    int noteOffset { 0 };
    float velocity { 1.0f };
    bool accent { false };
    float probability { 1.0f };
    int ratchetCount { 1 };
    float slideDuration { 0.0f };
    float gateWidth { 0.75f };
};

class Sequence {
public:
    static constexpr int maxSteps = 64;
    // Schema version history:
    //   1 = pre-release raw struct layout (ABI-dependent, not durable)
    //   2 = explicit fixed-width field serialization (durable, portable)
    // v1 is intentionally NOT supported for migration because it depended on
    // compiler ABI, struct padding, and bool representation. Pre-release state
    // written with v1 is treated as incompatible.
    static constexpr uint16_t currentVersion = 2;

    Sequence() = default;
    Sequence(int numSteps) : numSteps_(numSteps) {}

    [[nodiscard]] int size() const noexcept { return numSteps_; }

    void clear();
    void copyFrom(const Sequence& other);
    [[nodiscard]] Sequence copy() const;
    void paste(const Sequence& from);

    void rotateLeft(int steps = 1);
    void rotateRight(int steps = 1);
    void reverse();

    void transpose(int semitones);
    void octaveUp();
    void octaveDown();

    void shiftLeft(int steps = 1);
    void shiftRight(int steps = 1);

    void mutateSelected(int seed);
    void clearSelected();
    void regenerateBySeed(std::uint32_t seed);

    void copyTo(Sequence& dest) const;

    void setNumSteps(int n) { numSteps_ = juce::jlimit(1, maxSteps, n); }
    void setLength(int n) { setNumSteps(n); }
    [[nodiscard]] TimingMode getTimingMode() const noexcept { return timingMode_; }
    void setTimingMode(TimingMode m) noexcept { timingMode_ = m; }

    // Selection tracking (used by mutateSelected / clearSelected)
    [[nodiscard]] bool hasSelection() const noexcept
    {
        return selectedStart >= 0 && selectedEnd >= selectedStart
            && selectedStart < numSteps_;
    }
    void setSelectedRange(int start, int end) noexcept
    {
        selectedStart = juce::jlimit(0, maxSteps - 1, start);
        selectedEnd = juce::jlimit(0, maxSteps - 1, end);
        if (selectedEnd < selectedStart)
            selectedEnd = selectedStart;
    }
    void clearSelection() noexcept
    {
        selectedStart = 0;
        selectedEnd = -1;
    }
    [[nodiscard]] int getSelectedStart() const noexcept { return selectedStart; }
    [[nodiscard]] int getSelectedEnd() const noexcept { return selectedEnd; }

    // Step access
    [[nodiscard]] Step& getStep(int index) noexcept { return (*this)[index]; }
    [[nodiscard]] const Step& getStep(int index) const noexcept { return (*this)[index]; }
    void setStep(int index, const Step& step) noexcept { (*this)[index] = step; }

    // Serialization
    //
    // The binary format is versioned (magic + uint16 version + payload).
    // deserialize() accepts only currentVersion; v1 (raw struct layout) is
    // rejected because it depended on compiler ABI, struct padding and bool
    // representation. When the format is extended to v3+, bump currentVersion,
    // add a loadV3() plus a v2->v3 migration step in Sequence.cpp and route old
    // versions through the chain instead of rejecting them.
    void serialize(juce::MemoryBlock& mb) const;
    [[nodiscard]] static Sequence deserialize(const juce::MemoryBlock& mb);

    // Structural validation used by deserialize() and by preset loading so a
    // corrupt blob can be rejected without touching live state.
    [[nodiscard]] static bool isValidSerialization(const juce::MemoryBlock& mb);

    [[nodiscard]] Step& operator[](int index) noexcept
    {
        return steps_[juce::jlimit(0, maxSteps - 1, index)];
    }

    [[nodiscard]] const Step& operator[](int index) const noexcept
    {
        return steps_[juce::jlimit(0, maxSteps - 1, index)];
    }

private:
    // Per-version loaders. deserialize() validates and dispatches to one of
    // these; future versions add their own loader + migration step here.
    // v1 (raw struct layout) is intentionally NOT supported because it depended
    // on compiler ABI, struct padding, and bool representation.
    [[nodiscard]] static Sequence loadV2(const juce::MemoryBlock& mb);

    std::array<Step, maxSteps> steps_ {};
    int numSteps_ { 16 };
    TimingMode timingMode_ { TimingMode::sixteenth };
    int selectedStart { 0 };
    int selectedEnd { -1 };
};

} // namespace vstengine::generator
