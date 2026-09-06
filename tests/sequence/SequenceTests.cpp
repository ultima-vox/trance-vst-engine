#include "sequence/Sequence.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

static int testsRun = 0;
static int testsPassed = 0;

#define require(cond, msg)                                                   \
    do {                                                                     \
        ++testsRun;                                                          \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL: " << msg << " at line " << __LINE__ << "\n"; \
            std::exit(EXIT_FAILURE);                                         \
        }                                                                    \
        ++testsPassed;                                                       \
    } while (0)

int main()
{
    using namespace vstengine::sequence;

    // 1: Default construction
    {
        Sequence seq;
        require(seq.size() == 16, "default size is 16");
        require(seq[0].gate == false, "default step gate is false");
        require(seq[0].velocity == 1.0f, "default velocity is 1.0");
    }

    // 2: Construction with length
    {
        Sequence seq(32);
        require(seq.size() == 32, "32-step sequence");
    }

    // 3: setLength clamps to maxSteps
    {
        Sequence seq;
        seq.setLength(64);
        require(seq.size() == 64, "setLength 64");
        seq.setLength(200);
        require(seq.size() == 64, "setLength clamps to maxSteps");
    }

    // 4: Clear
    {
        Sequence seq;
        seq[0].gate = true;
        seq[0].noteOffset = 12;
        seq.clear();
        require(seq[0].gate == false, "clear resets gate");
        require(seq[0].noteOffset == 0, "clear resets noteOffset");
    }

    // 5: Copy
    {
        Sequence src;
        src[0].gate = true;
        src[0].noteOffset = 7;
        const Sequence dst = src.copy();
        require(dst.size() == src.size(), "copy keeps size");
        require(dst[0].gate == true, "copy keeps gate");
        require(dst[0].noteOffset == 7, "copy keeps noteOffset");
    }

    // 6: Paste
    {
        Sequence src;
        src[0].gate = true;
        src[0].velocity = 0.85f;
        Sequence dst(32);
        dst.paste(src);
        require(dst.size() == src.size(), "paste takes source size");
        require(dst[0].gate == true, "paste gate");
        require(std::abs(dst[0].velocity - 0.85f) < 0.001f, "paste velocity");
    }

    // 7: Rotate left
    {
        Sequence seq;
        seq[0].noteOffset = 1;
        seq[1].noteOffset = 2;
        seq[2].noteOffset = 3;
        seq.rotateLeft(1);
        require(seq[0].noteOffset == 2, "rotateLeft: old[1] -> new[0]");
        require(seq[1].noteOffset == 3, "rotateLeft: old[2] -> new[1]");
        require(seq[15].noteOffset == 1, "rotateLeft: old[0] wraps");
    }

    // 8: Rotate right
    {
        Sequence seq;
        seq[0].noteOffset = 1;
        seq[15].noteOffset = 15;
        seq.rotateRight(1);
        require(seq[0].noteOffset == 15, "rotateRight: old[15] -> new[0]");
        require(seq[1].noteOffset == 1, "rotateRight: old[0] -> new[1]");
    }

    // 9: Reverse (reverses all numSteps_ elements, not just initialized ones)
    {
        Sequence seq;
        seq[0].noteOffset = 1;
        seq[1].noteOffset = 2;
        seq[2].noteOffset = 3;
        seq.reverse();
        // reverse swaps [i] with [numSteps_-1-i], so index 0 gets old index 15
        // (default 0), index 14 gets old index 1 (2), index 15 gets old index 0 (1)
        require(seq[14].noteOffset == 2, "reverse: old[1] -> new[14]");
        require(seq[15].noteOffset == 1, "reverse: old[0] -> new[15]");
    }

    // 10: Transpose
    {
        Sequence seq;
        seq[0].noteOffset = 5;
        seq.transpose(7);
        require(seq[0].noteOffset == 12, "transpose +7");
        seq.transpose(-20);
        require(seq[0].noteOffset == -8, "transpose -20");
    }

    // 11: Octave up / down
    {
        Sequence seq;
        seq[0].noteOffset = 0;
        seq.octaveUp();
        require(seq[0].noteOffset == 12, "octaveUp: +12");
        seq.octaveDown();
        require(seq[0].noteOffset == 0, "octaveDown: -12");
    }

    // 12: Shift left / right fill with empty steps
    {
        Sequence seq;
        seq[0].noteOffset = 1;
        seq[1].noteOffset = 2;
        seq.shiftLeft(1);
        require(seq[0].noteOffset == 2, "shiftLeft moves data");
        require(seq[15].noteOffset == 0, "shiftLeft fills empty step");
        seq.shiftRight(1);
        require(seq[0].noteOffset == 0, "shiftRight fills empty step");
        require(seq[1].noteOffset == 2, "shiftRight restores data");
    }

    // 13: Mutate selected is deterministic and selection-aware
    {
        Sequence seq;
        seq[0].gate = true;
        seq[1].gate = true;
        seq[2].gate = true;
        seq.setSelectedRange(0, 1);

        const Sequence before = seq.copy();
        seq.mutateSelected(1234);
        const Sequence after = seq.copy();

        Sequence seq2 = before;
        seq2.mutateSelected(1234);
        for (int i = 0; i < 2; ++i) {
            require(after[i].gate == seq2[i].gate,
                    "mutateSelected deterministic gate");
            require(after[i].noteOffset == seq2[i].noteOffset,
                    "mutateSelected deterministic noteOffset");
            require(after[i].accent == seq2[i].accent,
                    "mutateSelected deterministic accent");
        }
        require(after[2].gate == before[2].gate,
                "mutateSelected leaves unselected steps alone");
    }

    // 14: Clear selected is selection-aware
    {
        Sequence seq;
        for (int i = 0; i < 8; ++i)
            seq[i].gate = true;
        seq.setSelectedRange(0, 3);
        seq.clearSelected();
        require(seq[0].gate == false, "clearSelected clears first selected");
        require(seq[3].gate == false, "clearSelected clears last selected");
        require(seq[4].gate == true, "clearSelected keeps outside selection");
    }

    // 15: Regenerate by seed is deterministic and keeps kick slot free
    {
        Sequence a;
        Sequence b;
        a.regenerateBySeed(0xABCDu);
        b.regenerateBySeed(0xABCDu);
        for (int i = 0; i < a.size(); ++i) {
            require(a[i].gate == b[i].gate, "regenerateBySeed same seed gates");
            require(a[i].noteOffset == b[i].noteOffset,
                    "regenerateBySeed same seed notes");
        }
        require(a[0].gate == false, "regenerateBySeed keeps downbeat free");
    }

    // 16: Serialization round-trip (steps, length, timing, selection)
    {
        Sequence original(32);
        original[0].gate = false;
        original[0].noteOffset = 7;
        original[0].velocity = 0.85f;
        original[0].accent = true;
        original[0].probability = 0.9f;
        original[0].ratchetCount = 3;
        original[0].slideDuration = 0.5f;
        original[0].gateWidth = 0.5f;
        original.setTimingMode(TimingMode::eighth);
        original.setSelectedRange(2, 5);

        juce::MemoryBlock mb;
        original.serialize(mb);

        const auto restored = Sequence::deserialize(mb);
        require(restored.size() == original.size(), "serialized size matches");
        require(restored[0].gate == false, "serialized gate");
        require(restored[0].noteOffset == 7, "serialized noteOffset");
        require(std::abs(restored[0].velocity - 0.85f) < 0.001f,
                "serialized velocity");
        require(restored[0].accent == true, "serialized accent");
        require(std::abs(restored[0].probability - 0.9f) < 0.001f,
                "serialized probability");
        require(restored[0].ratchetCount == 3, "serialized ratchetCount");
        require(restored.getTimingMode() == TimingMode::eighth,
                "serialized timing mode");
        require(restored.getSelectedStart() == 2, "serialized selection start");
        require(restored.getSelectedEnd() == 5, "serialized selection end");
    }

    // 17: Corrupt / short data returns a default sequence
    {
        const char small[4] = { 0, 0, 0, 0 };
        juce::MemoryBlock corrupt(small, sizeof(small));
        const auto restored = Sequence::deserialize(corrupt);
        require(restored.size() == 16, "corrupt data returns default");
    }

    // 18: Canonical timing helpers for every timing mode
    {
        require(stepsPerQuarterNote(TimingMode::eighth) == 2.0,
                "1/8 = 2 steps per quarter");
        require(stepsPerQuarterNote(TimingMode::sixteenth) == 4.0,
                "1/16 = 4 steps per quarter");
        require(stepsPerQuarterNote(TimingMode::thirtySecond) == 8.0,
                "1/32 = 8 steps per quarter");
        require(stepsPerQuarterNote(TimingMode::triplet) == 3.0,
                "triplet = 3 steps per quarter");
        require(std::abs(stepDurationInQuarterNotes(TimingMode::sixteenth) - 0.25)
                    < 1e-12,
                "1/16 step = quarter of a quarter note");
    }

    // 19: Wrong magic is rejected
    {
        Sequence seq(16);
        seq[0].gate = true;
        juce::MemoryBlock mb;
        seq.serialize(mb);
        auto* data = static_cast<char*>(mb.getData());
        data[0] = 'X'; // clobber the magic number
        require(!Sequence::isValidSerialization(mb),
                "wrong magic fails validation");
        require(Sequence::deserialize(mb).size() == 16,
                "wrong magic returns default");
        require(Sequence::deserialize(mb)[0].gate == false,
                "wrong magic returns cleared steps");
    }

    // 20: Unsupported future version is rejected cleanly
    {
        Sequence seq(16);
        juce::MemoryBlock mb;
        seq.serialize(mb);
        auto* data = static_cast<char*>(mb.getData());
        const std::uint16_t futureVersion = 999;
        std::memcpy(data + sizeof(std::uint32_t), &futureVersion,
                    sizeof(std::uint16_t));
        require(!Sequence::isValidSerialization(mb),
                "future version fails validation");
        require(Sequence::deserialize(mb).size() == 16,
                "future version returns default");
    }

    // 21: Invalid timing mode byte is rejected
    {
        Sequence seq(16);
        seq.setTimingMode(TimingMode::eighth);
        juce::MemoryBlock mb;
        seq.serialize(mb);
        auto* data = static_cast<char*>(mb.getData());
        const std::uint8_t badTiming = 200;
        std::memcpy(data + sizeof(std::uint32_t) + sizeof(std::uint16_t)
                        + sizeof(int),
                    &badTiming, sizeof(std::uint8_t));
        require(!Sequence::isValidSerialization(mb),
                "invalid timing mode fails validation");
        require(Sequence::deserialize(mb).getTimingMode()
                    == TimingMode::sixteenth,
                "invalid timing mode returns default");
    }

    // 22: Truncated payload is rejected
    {
        Sequence seq(16);
        seq[0].gate = true;
        juce::MemoryBlock mb;
        seq.serialize(mb);
        mb.setSize(mb.getSize() - 20); // cut into the steps payload
        require(!Sequence::isValidSerialization(mb),
                "truncated steps payload fails validation");
        require(Sequence::deserialize(mb).size() == 16,
                "truncated payload returns default");
    }

    // 23: Non-finite step values are sanitized
    {
        Sequence seq(16);
        seq[0].velocity = std::numeric_limits<float>::quiet_NaN();
        seq[0].probability = std::numeric_limits<float>::infinity();
        seq[0].gateWidth = 4.0f;
        seq[0].ratchetCount = 99;
        seq[0].noteOffset = 500;
        seq[0].slideDuration = -3.0f;
        juce::MemoryBlock mb;
        seq.serialize(mb);
        const auto restored = Sequence::deserialize(mb);
        require(std::isfinite(restored[0].velocity), "velocity sanitized");
        require(restored[0].velocity >= 0.0f && restored[0].velocity <= 1.0f,
                "velocity in range");
        require(restored[0].probability >= 0.0f && restored[0].probability <= 1.0f,
                "probability in range");
        require(restored[0].gateWidth >= 0.0f && restored[0].gateWidth <= 1.0f,
                "gateWidth in range");
        require(restored[0].ratchetCount >= 1 && restored[0].ratchetCount <= 8,
                "ratchetCount in range");
        require(restored[0].noteOffset >= -24 && restored[0].noteOffset <= 24,
                "noteOffset in range");
        require(restored[0].slideDuration == 0.0f, "slideDuration sanitized");
    }

    // 24: Round-trip serialization preserves all fields (v2 explicit schema)
    {
        Sequence seq(24);
        seq.setTimingMode(TimingMode::triplet);
        seq.setSelectedRange(3, 17);
        for (int i = 0; i < seq.size(); ++i) {
            auto& step = seq[i];
            step.gate = (i % 3) != 0;
            step.noteOffset = (i % 13) - 6;
            step.velocity = 0.1f + 0.07f * (i % 11);
            step.accent = (i % 5) == 0;
            step.probability = (i % 7) == 0 ? 0.5f : 1.0f;
            step.ratchetCount = 1 + (i % 8);
            step.slideDuration = (i % 4) == 0 ? 2.0f : 0.0f;
            step.gateWidth = 0.1f * (i % 8);
        }

        juce::MemoryBlock mb;
        seq.serialize(mb);
        const auto restored = Sequence::deserialize(mb);

        require(restored.size() == seq.size(), "round-trip: size");
        require(restored.getTimingMode() == TimingMode::triplet,
                "round-trip: timing mode");
        require(restored.getSelectedStart() == 3, "round-trip: selection start");
        require(restored.getSelectedEnd() == 17, "round-trip: selection end");

        for (int i = 0; i < seq.size(); ++i) {
            require(restored[i].gate == seq[i].gate,
                    "round-trip: gate");
            require(restored[i].noteOffset == seq[i].noteOffset,
                    "round-trip: noteOffset");
            require(std::abs(restored[i].velocity - seq[i].velocity) < 0.001f,
                    "round-trip: velocity");
            require(restored[i].accent == seq[i].accent,
                    "round-trip: accent");
            require(std::abs(restored[i].probability - seq[i].probability) < 0.001f,
                    "round-trip: probability");
            require(restored[i].ratchetCount == seq[i].ratchetCount,
                    "round-trip: ratchetCount");
            require(std::abs(restored[i].slideDuration - seq[i].slideDuration) < 0.001f,
                    "round-trip: slideDuration");
            require(std::abs(restored[i].gateWidth - seq[i].gateWidth) < 0.001f,
                    "round-trip: gateWidth");
        }
    }

    // 25: shouldPlayStep is deterministic and advances RNG once per step
    {
        // Same seed + same sequence = same decisions
        ProbabilityState ps1 { 42 };
        ProbabilityState ps2 { 42 };

        float probabilities[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.33f };
        bool decisions1[6], decisions2[6];

        for (int i = 0; i < 6; ++i) {
            decisions1[i] = shouldPlayStep(ps1, probabilities[i]);
            decisions2[i] = shouldPlayStep(ps2, probabilities[i]);
        }

        for (int i = 0; i < 6; ++i) {
            require(decisions1[i] == decisions2[i],
                    "deterministic: same seed = same decisions");
        }

        // probability 0.0 never plays (except gate-off steps which are handled
        // by caller, but shouldPlayStep itself returns false for prob 0)
        require(decisions1[0] == false, "probability 0 never plays");

        // probability 1.0 always plays
        require(decisions1[4] == true, "probability 1 always plays");
    }

    // 26: Different seeds can produce different decision masks
    {
        ProbabilityState ps1 { 12345 };
        ProbabilityState ps2 { 54321 };

        // Use a mix of gated and ungated steps to prove state advances per step
        bool anyDifferent = false;
        for (int i = 0; i < 16; ++i) {
            float prob = 0.5f;
            bool d1 = shouldPlayStep(ps1, prob);
            bool d2 = shouldPlayStep(ps2, prob);
            if (d1 != d2)
                anyDifferent = true;
        }
        require(anyDifferent, "different seeds produce different masks");
    }

    // 27: v1 (raw struct layout) is rejected
    {
        juce::MemoryBlock mb;
        // Construct a v1-style payload: magic + version=1 + raw struct data
        const uint32_t v1Magic = 0x53514551;
        const uint16_t v1Version = 1;
        mb.append(&v1Magic, sizeof(v1Magic));
        mb.append(&v1Version, sizeof(v1Version));
        // Add some garbage data
        std::vector<char> garbage(256, 0);
        mb.append(garbage.data(), garbage.size());

        require(!Sequence::isValidSerialization(mb),
                "v1 raw struct layout is rejected");
        require(Sequence::deserialize(mb).size() == 16,
                "v1 returns default");
    }

    // 28: Empty selection (clearSelection / default) round-trips correctly
    {
        Sequence seq(16);
        seq.clearSelection(); // canonical no-selection: start=0, end=-1
        require(seq.getSelectedStart() == 0, "clearSelection: start=0");
        require(seq.getSelectedEnd() == -1, "clearSelection: end=-1");
        require(!seq.hasSelection(), "clearSelection: hasSelection false");

        juce::MemoryBlock mb;
        seq.serialize(mb);
        const auto restored = Sequence::deserialize(mb);

        require(restored.getSelectedStart() == 0, "round-trip empty: start=0");
        require(restored.getSelectedEnd() == -1, "round-trip empty: end=-1");
        require(!restored.hasSelection(), "round-trip empty: hasSelection false");
    }

    // 29: Valid selection round-trips correctly
    {
        Sequence seq(32);
        seq.setSelectedRange(5, 20);
        require(seq.getSelectedStart() == 5, "setSelectedRange: start=5");
        require(seq.getSelectedEnd() == 20, "setSelectedRange: end=20");
        require(seq.hasSelection(), "setSelectedRange: hasSelection true");

        juce::MemoryBlock mb;
        seq.serialize(mb);
        const auto restored = Sequence::deserialize(mb);

        require(restored.getSelectedStart() == 5, "round-trip valid: start=5");
        require(restored.getSelectedEnd() == 20, "round-trip valid: end=20");
        require(restored.hasSelection(), "round-trip valid: hasSelection true");
    }

    // 30: Malformed selection (start > end) defaults to no selection
    {
        Sequence seq(16);
        seq.setSelectedRange(3, 10); // valid selection first
        juce::MemoryBlock mb;
        seq.serialize(mb);

        // Corrupt the selection: make start=10, end=3 (invalid: start > end)
        auto* data = static_cast<std::uint8_t*>(mb.getData());
        // Selection is at the end of the buffer. Compute offset:
        // header(16 steps * 26 bytes) + selection header
        const size_t hdr = 4 + 2 + 4 + 1; // 11 bytes
        const size_t stepBytes = 16 * 26;  // 416 bytes
        const size_t selOffset = hdr + stepBytes;

        // Write invalid selection: start=10, end=3
        data[selOffset + 0] = 10; data[selOffset + 1] = 0;
        data[selOffset + 2] = 0;  data[selOffset + 3] = 0;
        data[selOffset + 4] = 3;  data[selOffset + 5] = 0;
        data[selOffset + 6] = 0;  data[selOffset + 7] = 0;

        const auto restored = Sequence::deserialize(mb);
        require(restored.getSelectedStart() == 0, "malformed: defaults to start=0");
        require(restored.getSelectedEnd() == -1, "malformed: defaults to end=-1");
        require(!restored.hasSelection(), "malformed: hasSelection false");
    }

    // 31: Malformed selection (out of range) defaults to no selection
    {
        Sequence seq(16);
        juce::MemoryBlock mb;
        seq.serialize(mb);

        auto* data = static_cast<std::uint8_t*>(mb.getData());
        const size_t hdr = 4 + 2 + 4 + 1;
        const size_t stepBytes = 16 * 26;
        const size_t selOffset = hdr + stepBytes;

        // Write out-of-range selection: start=0, end=100 (beyond numSteps)
        data[selOffset + 0] = 0;   data[selOffset + 1] = 0;
        data[selOffset + 2] = 0;   data[selOffset + 3] = 0;
        data[selOffset + 4] = 100; data[selOffset + 5] = 0;
        data[selOffset + 6] = 0;   data[selOffset + 7] = 0;

        const auto restored = Sequence::deserialize(mb);
        require(restored.getSelectedStart() == 0, "out-of-range: defaults to start=0");
        require(restored.getSelectedEnd() == -1, "out-of-range: defaults to end=-1");
        require(!restored.hasSelection(), "out-of-range: hasSelection false");
    }

    // 32: Explicit round-trip across all timing modes and edge counts
    {
        TimingMode modes[] = {
            TimingMode::sixteenth, TimingMode::eighth,
            TimingMode::thirtySecond, TimingMode::triplet
        };
        int counts[] = { 1, 16, 32, 64 };

        for (auto mode : modes) {
            for (int count : counts) {
                Sequence seq(count);
                seq.setTimingMode(mode);
                seq.setSelectedRange(0, count - 1);
                for (int i = 0; i < count; ++i) {
                    seq[i].gate = (i % 2) == 0;
                    seq[i].noteOffset = i - count / 2;
                    seq[i].velocity = 0.5f;
                    seq[i].probability = 0.9f;
                    seq[i].ratchetCount = 1 + (i % 8);
                    seq[i].slideDuration = 1.5f;
                    seq[i].gateWidth = 0.8f;
                }

                juce::MemoryBlock mb;
                seq.serialize(mb);
                const auto restored = Sequence::deserialize(mb);

                require(restored.size() == count,
                        "round-trip: count preserved");
                require(restored.getTimingMode() == mode,
                        "round-trip: timing mode preserved");
            }
        }
    }

    std::cout << "Sequence tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}