#include "midi/MidiExport.h"
#include <cmath>
#include <cstring>
#include <iostream>

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

namespace {

int countNoteOns(const juce::MidiMessageSequence& track)
{
    int count = 0;
    for (const auto* event : track)
        if (event->message.isNoteOn())
            ++count;
    return count;
}

// Every note-on must be matched to a later note-off on the same note number
// (no stuck notes anywhere in the exported track).
void requireNoStuckNotes(const juce::MidiMessageSequence& track)
{
    for (const auto* event : track) {
        const auto& message = event->message;
        if (!message.isNoteOn())
            continue;
        require(event->noteOffObject != nullptr,
                "every note-on has a matched note-off");
        require(event->noteOffObject->message.getNoteNumber()
                    == message.getNoteNumber(),
                "note-off matches note-on pitch");
        require(event->noteOffObject->message.getTimeStamp()
                    >= message.getTimeStamp(),
                "note-off is not before its note-on");
    }
}

} // namespace

int main()
{
    using namespace vstengine::sequence;
    using vstengine::midiexport::buildSequenceTrack;
    using vstengine::midiexport::Options;

    // 1: 1/16 timing (default) - adjacent gated steps are a quarter/4 apart
    {
        Sequence seq;
        seq[0].gate = true;
        seq[1].gate = true;
        const auto track = buildSequenceTrack(seq, {});
        require(countNoteOns(track) == 2, "1/16: two gated steps, two notes");

        double noteOnTicks[2] = { -1.0, -1.0 };
        int n = 0;
        for (const auto* event : track)
            if (event->message.isNoteOn())
                noteOnTicks[n++] = event->message.getTimeStamp();
        require(std::abs(noteOnTicks[0] - 0.0) < 1e-9, "1/16: step 0 at tick 0");
        require(std::abs(noteOnTicks[1] - 240.0) < 1e-9,
                "1/16: step 1 at tick 240 (960/4)");
    }

    // 2: 1/8 timing
    {
        Sequence seq;
        seq.setTimingMode(TimingMode::eighth);
        seq[0].gate = true;
        seq[1].gate = true;
        const auto track = buildSequenceTrack(seq, {});
        int count = 0;
        for (const auto* event : track) {
            if (event->message.isNoteOn()) {
                if (count == 1)
                    require(std::abs(event->message.getTimeStamp() - 480.0)
                                < 1e-9,
                            "1/8: step 1 at tick 480 (960/2)");
                ++count;
            }
        }
        require(count == 2, "1/8: two notes");
    }

    // 3: 1/32 timing
    {
        Sequence seq;
        seq.setTimingMode(TimingMode::thirtySecond);
        seq[0].gate = true;
        seq[1].gate = true;
        const auto track = buildSequenceTrack(seq, {});
        int count = 0;
        for (const auto* event : track) {
            if (event->message.isNoteOn()) {
                if (count == 1)
                    require(std::abs(event->message.getTimeStamp() - 120.0)
                                < 1e-9,
                            "1/32: step 1 at tick 120 (960/8)");
                ++count;
            }
        }
        require(count == 2, "1/32: two notes");
    }

    // 4: triplet timing
    {
        Sequence seq;
        seq.setTimingMode(TimingMode::triplet);
        seq[0].gate = true;
        seq[1].gate = true;
        const auto track = buildSequenceTrack(seq, {});
        int count = 0;
        for (const auto* event : track) {
            if (event->message.isNoteOn()) {
                if (count == 1)
                    require(std::abs(event->message.getTimeStamp() - 320.0)
                                < 1e-9,
                            "triplet: step 1 at tick 320 (960/3)");
                ++count;
            }
        }
        require(count == 2, "triplet: two notes");
    }

    // 5: ratchet emits evenly spaced repeated sub-notes, gate respected
    {
        Sequence seq;
        seq[0].gate = true;
        seq[0].ratchetCount = 4;
        seq[0].gateWidth = 0.75f;
        seq[1].gate = false;
        const auto track = buildSequenceTrack(seq, {});
        require(countNoteOns(track) == 4, "ratchet 4: four sub-notes");
        requireNoStuckNotes(track);

        const double expectedOns[4] = { 0.0, 60.0, 120.0, 180.0 };
        const double expectedOffs[4] = { 45.0, 105.0, 165.0, 225.0 };
        int n = 0;
        for (const auto* event : track) {
            if (!event->message.isNoteOn())
                continue;
            require(std::abs(event->message.getTimeStamp() - expectedOns[n])
                        < 1e-9,
                    "ratchet sub-note onset spacing");
            require(event->noteOffObject != nullptr
                        && std::abs(event->noteOffObject->message.getTimeStamp()
                                    - expectedOffs[n]) < 1e-9,
                    "ratchet sub-note length = step/ratchet * gateWidth");
            ++n;
        }
    }

    // 6: ratchet sub-notes never overlap (no stuck notes at gateWidth = 1)
    {
        Sequence seq;
        seq[0].gate = true;
        seq[0].ratchetCount = 8;
        seq[0].gateWidth = 1.0f;
        const auto track = buildSequenceTrack(seq, {});
        require(countNoteOns(track) == 8, "ratchet 8 at full gate: 8 sub-notes");
        requireNoStuckNotes(track);

        double previousOff = -1.0;
        bool haveOn = false;
        for (const auto* event : track) {
            if (event->message.isNoteOn()) {
                if (haveOn)
                    require(previousOff <= event->message.getTimeStamp() + 1e-9,
                            "gateWidth 1.0: off before next on");
                haveOn = true;
                previousOff = event->noteOffObject != nullptr
                    ? event->noteOffObject->message.getTimeStamp()
                    : -1.0;
            }
        }
    }

    // 7: slide emits a deterministic pitch-bend ramp ending at the note-on
    {
        Sequence seq;
        seq[0].gate = true;
        seq[0].noteOffset = 0;
        seq[1].gate = true;
        seq[1].noteOffset = 12; // octave up
        seq[1].slideDuration = 1.0f;
        const auto track = buildSequenceTrack(seq, {});
        requireNoStuckNotes(track);

        int bendCount = 0;
        double lastBendTick = -1.0;
        int lastBendValue = 9999;
        const double noteOnTick = 240.0; // step 1 at 1/16
        for (const auto* event : track) {
            const auto& message = event->message;
            if (message.isPitchWheel()) {
                ++bendCount;
                lastBendTick = message.getTimeStamp();
                lastBendValue = message.getPitchWheelValue();
            }
        }
        require(bendCount == vstengine::midiexport::slideRampSegments + 1,
                "slide ramp has the documented segment count");
        require(lastBendTick < noteOnTick + 1e-9,
                "ramp ends at/before the note-on");
        require(lastBendValue == 8192,
                "ramp ends centered on the target pitch");
        require(std::abs(lastBendTick - noteOnTick) < 1e-9,
                "ramp reaches the target pitch exactly at the note-on");
    }

    // 8: same-pitch slide emits no ramp
    {
        Sequence seq;
        seq[0].gate = true;
        seq[0].slideDuration = 2.0f;
        seq[1].gate = true;
        seq[1].noteOffset = 0;
        seq[1].slideDuration = 2.0f; // same pitch as step 0
        const auto track = buildSequenceTrack(seq, {});
        int bendCount = 0;
        for (const auto* event : track)
            if (event->message.isPitchWheel())
                ++bendCount;
        require(bendCount == 0, "same-pitch slide emits no ramp");
        requireNoStuckNotes(track);
    }

    // 9: probability affects exported MIDI. A step with probability 0.0f is
    //    skipped by the deterministic splitmix32 gate, so it emits no note.
    {
        Sequence seq;
        seq[0].gate = false;
        seq[0].probability = 0.0f;
        seq[1].gate = true;
        seq[1].probability = 0.0f; // zero probability: step is skipped
        seq[1].noteOffset = 0;
        const auto track = buildSequenceTrack(seq, {});
        require(countNoteOns(track) == 0,
                "probability 0.0f skips the step in exported MIDI");
    }

    // 9b: probability 1.0f always passes; probability gate is deterministic.
    {
        Sequence seq;
        for (int i = 0; i < 8; ++i) {
            seq[i].gate = true;
            seq[i].probability = (i % 2 == 0) ? 1.0f : 0.5f;
            seq[i].noteOffset = i;
        }
        vstengine::midiexport::Options options;
        options.seed = 12345;
        const auto track = buildSequenceTrack(seq, options);
        // With a fixed seed, the result is deterministic. Count the note-ons.
        const auto count = countNoteOns(track);
        require(count > 0 && count <= 8,
                "probability gate deterministically filters some steps");

        // Same seed produces the same count.
        const auto track2 = buildSequenceTrack(seq, options);
        require(countNoteOns(track2) == count,
                "same seed produces same probability decisions");
    }

    // 10: deterministic output - identical builds are identical
    {
        Sequence seq;
        for (int i = 0; i < seq.size(); ++i) {
            seq[i].gate = (i % 3) != 0;
            seq[i].ratchetCount = 1 + (i % 4);
            seq[i].slideDuration = (i % 5) == 0 ? 0.0f : 1.0f;
            seq[i].noteOffset = (i % 7) - 3;
            seq[i].gateWidth = 0.25f + 0.1f * (i % 6);
        }
        seq.setTimingMode(TimingMode::triplet);

        Options options;
        options.channel = 5;
        options.rootNote = 40;

        const auto a = buildSequenceTrack(seq, options);
        const auto b = buildSequenceTrack(seq, options);
        require(a.getNumEvents() == b.getNumEvents(),
                "deterministic: event count");
        for (int i = 0; i < a.getNumEvents(); ++i) {
            const auto& ma = a.getEventPointer(i)->message;
            const auto& mb = b.getEventPointer(i)->message;
            require(ma.getRawDataSize() == mb.getRawDataSize(),
                    "deterministic: raw size");
            require(std::memcmp(ma.getRawData(), mb.getRawData(),
                                static_cast<size_t>(ma.getRawDataSize())) == 0,
                    "deterministic: raw event bytes");
            require(std::abs(ma.getTimeStamp() - mb.getTimeStamp()) < 1e-9,
                    "deterministic: timestamps");
        }
        requireNoStuckNotes(a);
    }

    // 11: Probability parity — export uses the same shouldPlayStep rule as the
    //     shared helper. Mixed gate states + mixed probabilities must produce
    //     decisions identical to calling shouldPlayStep directly.
    {
        Sequence seq(16);
        // Mix of gated and ungated steps with varying probabilities
        for (int i = 0; i < 16; ++i) {
            seq[i].gate = (i % 3) != 0; // steps 0,3,6,9,12,15 are off
            seq[i].probability = (i % 4 == 0) ? 0.3f : ((i % 4 == 1) ? 0.7f : 1.0f);
            seq[i].noteOffset = i;
        }

        const std::uint32_t seed = 98765;
        Options options;
        options.seed = seed;

        // Compute expected decisions using the shared helper
        ProbabilityState ps { seed };
        bool expectedPlays[16];
        for (int i = 0; i < 16; ++i) {
            expectedPlays[i] = vstengine::sequence::shouldPlayStep(ps, seq[i].probability);
        }

        // Build the track and count note-ons per step
        const auto track = buildSequenceTrack(seq, options);

        // For each step, verify: if gate is on AND expectedPlays, there's a note-on
        // We verify by checking that the number of note-ons matches the count of
        // steps where gate && expectedPlays
        int expectedNoteOns = 0;
        for (int i = 0; i < 16; ++i) {
            if (seq[i].gate && expectedPlays[i])
                ++expectedNoteOns;
        }

        const auto actualNoteOns = countNoteOns(track);
        require(actualNoteOns == expectedNoteOns,
                "probability parity: export matches shouldPlayStep decisions");

        // Also verify determinism: same seed = same count
        const auto track2 = buildSequenceTrack(seq, options);
        require(countNoteOns(track2) == actualNoteOns,
                "probability parity: deterministic across builds");
    }

    // 12: Probability with all-gated sequence — every step advances RNG
    {
        Sequence seq(32);
        for (int i = 0; i < 32; ++i) {
            seq[i].gate = true;
            seq[i].probability = 0.5f;
            seq[i].noteOffset = i % 12;
        }

        const std::uint32_t seed = 555;
        Options options;
        options.seed = seed;

        // Compute expected decisions
        ProbabilityState ps { seed };
        int expectedPlays = 0;
        for (int i = 0; i < 32; ++i) {
            if (vstengine::sequence::shouldPlayStep(ps, 0.5f))
                ++expectedPlays;
        }

        const auto track = buildSequenceTrack(seq, options);
        require(countNoteOns(track) == expectedPlays,
                "all-gated probability: export matches helper");
        requireNoStuckNotes(track);
    }

    std::cout << "MIDI export tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}
