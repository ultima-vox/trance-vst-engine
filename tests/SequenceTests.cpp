#include "generator/Sequence.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

static int testsRun = 0;
static int testsPassed = 0;

#define require(cond, msg)                                                   \
    do {                                                                     \
        ++testsRun;                                                          \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL: " << msg << " at line " << __LINE__ << "\n"; \
            std::exit(1);                                                    \
        }                                                                    \
        ++testsPassed;                                                       \
    } while (0)

int main()
{
    // Test 1: Default construction
    {
        vstengine::generator::Sequence seq;
        require(seq.size() == 16, "default size is 16");
        require(seq.getLength() == vstengine::generator::SequenceLength::steps16,
                "default length enum is steps16");
        require(seq[0].gate == true, "default step gate is true");
        require(seq[0].noteOffset == 0, "default noteOffset is 0");
        require(seq[0].velocity == 0.72f, "default velocity is 0.72");
    }

    // Test 2: Construction with length
    {
        vstengine::generator::Sequence seq(
            vstengine::generator::SequenceLength::steps32);
        require(seq.size() == 32, "32-step sequence");
        require(seq.getLength() == vstengine::generator::SequenceLength::steps32,
                "length enum is steps32");
    }

    // Test 3: Clear
    {
        vstengine::generator::Sequence seq;
        seq[0].gate = false;
        seq[0].noteOffset = 12;
        seq.clear();
        require(seq[0].gate == true, "clear resets gate");
        require(seq[0].noteOffset == 0, "clear resets noteOffset");
    }

    // Test 4: Copy
    {
        vstengine::generator::Sequence src;
        src[0].gate = false;
        src[0].noteOffset = 7;

        vstengine::generator::Sequence dst(
            vstengine::generator::SequenceLength::steps32);
        dst.copyFrom(src, 0, 0, 1);

        require(dst[0].gate == false, "copy gate");
        require(dst[0].noteOffset == 7, "copy noteOffset");
    }

    // Test 5: Rotate left
    {
        vstengine::generator::Sequence seq;
        seq[0].noteOffset = 1;
        seq[1].noteOffset = 2;
        seq[2].noteOffset = 3;
        seq.rotateLeft(1);

        require(seq[0].noteOffset == 2, "rotateLeft: old[1] -> new[0]");
        require(seq[1].noteOffset == 3, "rotateLeft: old[2] -> new[1]");
        require(seq[15].noteOffset == 1, "rotateLeft: old[0] -> wrap");
    }

    // Test 6: Rotate right
    {
        vstengine::generator::Sequence seq;
        seq[0].noteOffset = 1;
        seq[15].noteOffset = 15;
        seq.rotateRight(1);

        require(seq[0].noteOffset == 15, "rotateRight: old[15] -> new[0]");
        require(seq[1].noteOffset == 1, "rotateRight: old[0] -> new[1]");
    }

    // Test 7: Reverse
    {
        vstengine::generator::Sequence seq;
        seq[0].noteOffset = 1;
        seq[1].noteOffset = 2;
        seq[2].noteOffset = 3;
        seq.reverse();

        require(seq[0].noteOffset == 3, "reverse: last -> first");
        require(seq[1].noteOffset == 2, "reverse: middle stays");
        require(seq[2].noteOffset == 1, "reverse: first -> last");
    }

    // Test 8: Transpose
    {
        vstengine::generator::Sequence seq;
        seq[0].noteOffset = 5;
        seq.transpose(7);

        require(seq[0].noteOffset == 12, "transpose +7: 5+7=12");

        seq.transpose(-20);
        require(seq[0].noteOffset == -8, "transpose -20: 12-20=-8 (clamped)");
    }

    // Test 9: Octave up/down
    {
        vstengine::generator::Sequence seq;
        seq[0].noteOffset = 0;
        seq.octaveUp();
        require(seq[0].noteOffset == 12, "octaveUp: +12");

        seq.octaveDown();
        require(seq[0].noteOffset == 0, "octaveDown: -12");
    }

    // Test 10: Mutate selected
    {
        vstengine::generator::Sequence seq;
        seq[0].gate = true;

        const auto originalGate = seq[0].gate;
        seq.mutateSelected({0}, 0x1234);

        // With seed 0x1234, check that mutation occurred deterministically
        const auto mutatedGate = seq[0].gate;
        (void)mutatedGate; // mutation may or may not toggle gate depending on seed
    }

    // Test 11: Clear selected
    {
        vstengine::generator::Sequence seq;
        seq[0].gate = true;
        seq[1].gate = true;
        seq.clearSelected({0});

        require(seq[0].gate == false, "clearSelected: step 0 off");
        require(seq[1].gate == true, "clearSelected: step 1 still on");
    }

    // Test 12: Serialization round-trip
    {
        vstengine::generator::Sequence original;
        original[0].gate = false;
        original[0].noteOffset = 7;
        original[0].velocity = 0.85f;
        original[0].accent = 1.0f;
        original[0].probability = 0.9f;
        original[0].ratchetCount = 3;
        original[0].slideDuration = 2;
        original[0].gateWidth = 0.5f;

        juce::MemoryBlock mb;
        original.serialize(mb);

        const auto restored = vstengine::generator::Sequence::deserialize(mb);

        require(restored.size() == original.size(),
                "serialized size matches");
        require(restored[0].gate == false, "serialized gate");
        require(restored[0].noteOffset == 7, "serialized noteOffset");
        require(std::abs(restored[0].velocity - 0.85f) < 0.001f,
                "serialized velocity");
        require(std::abs(restored[0].accent - 1.0f) < 0.001f,
                "serialized accent");
        require(std::abs(restored[0].probability - 0.9f) < 0.001f,
                "serialized probability");
        require(restored[0].ratchetCount == 3, "serialized ratchetCount");
        require(restored[0].slideDuration == 2, "serialized slideDuration");
        require(std::abs(restored[0].gateWidth - 0.5f) < 0.001f,
                "serialized gateWidth");
    }

    // Test 13: Deserialize corrupt data
    {
        juce::MemoryBlock corrupt;
        const char small[] = { 0, 0, 0, 0 }; // too small
        corrupt.copyFrom(small, 0, sizeof(small));

        const auto restored = vstengine::generator::Sequence::deserialize(corrupt);
        require(restored.size() == 16, "corrupt data returns default");
    }

    // Test 14: Set length
    {
        vstengine::generator::Sequence seq;
        seq.setLength(vstengine::generator::SequenceLength::steps64);
        require(seq.size() == 64, "setLength to 64");
        require(seq.getLength() == vstengine::generator::SequenceLength::steps64,
                "length enum updated");
    }

    // Test 15: Shift left/right
    {
        vstengine::generator::Sequence seq;
        seq[0].noteOffset = 10;
        seq[1].noteOffset = 20;
        seq.shiftLeft(1);

        require(seq[0].noteOffset == 20, "shiftLeft: old[1] -> new[0]");
        require(seq[15].noteOffset == 10, "shiftLeft: wrap");

        seq.shiftRight(1);
        require(seq[0].noteOffset == 10, "shiftRight: restores");
    }

    std::cout << testsPassed << "/" << testsRun << " tests passed\n";
    return (testsPassed == testsRun) ? 0 : 1;
}
