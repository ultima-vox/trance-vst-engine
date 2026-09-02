#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

namespace vstengine::generator {

// Step data: what happens at each sequencer step
struct Step {
    bool gate { true };           // note on/off
    int noteOffset { 0 };         // semitone offset from root
    float velocity { 0.72f };     // 0-1
    float accent { 0.0f };        // 0-1, 0 = no accent
    float probability { 1.0f };   // 0-1, chance note fires
    int ratchetCount { 1 };       // repeat note N times per step
    int slideDuration { 0 };      // steps to slide over (0 = no slide)
    float gateWidth { 0.62f };    // 0-1, fraction of step duration note is held
};

// Timing mode
enum class TimingMode : int {
    sixteenth = 0,
    eighth,
    thirtySecond,
    triplet
};

// Sequence length
enum class SequenceLength : int {
    steps16 = 0,
    steps32,
    steps64
};

// Canonical sequence: fixed-size array, length determined by SequenceLength
class Sequence {
public:
    static constexpr size_t maxSteps = 64;

    // Constructors
    Sequence();
    explicit Sequence(SequenceLength len);

    // Access
    [[nodiscard]] size_t size() const noexcept { return length; }
    [[nodiscard]] SequenceLength getLength() const noexcept { return seqLength; }
    [[nodiscard]] TimingMode getTiming() const noexcept { return timing; }

    // Modifiers
    void setLength(SequenceLength len);
    void setTiming(TimingMode t);

    // Step access
    Step& operator[](size_t i) { return steps[i]; }
    const Step& operator[](size_t i) const { return steps[i]; }

    // Operations
    void clear();
    void copyFrom(const Sequence& other, size_t destStart, size_t srcStart, size_t count);
    void rotateLeft(size_t count = 1);
    void rotateRight(size_t count = 1);
    void reverse();
    void transpose(int semitones);
    void octaveUp();
    void octaveDown();
    void shiftLeft(size_t count = 1);   // circular shift left
    void shiftRight(size_t count = 1);  // circular shift right
    void mutateSelected(const std::vector<size_t>& indices, uint32_t seed);
    void clearSelected(const std::vector<size_t>& indices);
    void regenerateBySeed(uint32_t seed); // re-generate using style rules (stub for Phase 14)

    // Persistence helpers
    void serialize(juce::MemoryBlock& dest) const;
    static Sequence deserialize(const juce::MemoryBlock& src);

private:
    std::array<Step, maxSteps> steps {};
    size_t length { 16 };
    SequenceLength seqLength { SequenceLength::steps16 };
    TimingMode timing { TimingMode::sixteenth };
};

} // namespace vstengine::generator
