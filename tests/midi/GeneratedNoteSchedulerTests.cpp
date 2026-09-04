#include "midi/GeneratedNoteScheduler.h"
#include "sequence/Sequence.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int testsRun = 0;
int testsPassed = 0;

#define require(cond, msg)                                                   \
    do {                                                                     \
        ++testsRun;                                                          \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL: " << msg << " at line " << __LINE__ << "\n"; \
            std::exit(EXIT_FAILURE);                                         \
        }                                                                    \
        ++testsPassed;                                                       \
    } while (0)

int countNoteOns(const juce::MidiBuffer& midi)
{
    int count = 0;
    for (const auto metadata : midi)
        if (metadata.getMessage().isNoteOn())
            ++count;
    return count;
}

int countNoteOffs(const juce::MidiBuffer& midi)
{
    int count = 0;
    for (const auto metadata : midi)
        if (metadata.getMessage().isNoteOff())
            ++count;
    return count;
}

} // namespace

int main()
{
    using vstengine::midi::GeneratedNoteScheduler;
    using vstengine::midi::TransportFrame;
    using vstengine::sequence::Sequence;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    // Helper: all-gated 16-step sequence, ratchet=1, probability=1.
    auto makeSequence = [] {
        Sequence seq(16);
        seq.setTimingMode(vstengine::sequence::TimingMode::sixteenth);
        for (int i = 0; i < 16; ++i) {
            seq[i].gate = true;
            seq[i].noteOffset = 0;
            seq[i].velocity = 0.9f;
            seq[i].accent = false;
            seq[i].probability = 1.0f;
            seq[i].ratchetCount = 1;
            seq[i].slideDuration = 0.0f;
            seq[i].gateWidth = 0.5f;
        }
        return seq;
    };

    // 1: PPQ path schedules one note per step boundary at 120 BPM 1/16 grid.
    {
        const auto seq = makeSequence();
        GeneratedNoteScheduler s;
        s.reset(sampleRate);
        s.reseedProbability(1234);

        // Step duration in quarters = 0.25; at 120 BPM = 0.5 s = 24000 samples.
        juce::MidiBuffer midi;
        TransportFrame frame;
        frame.playing = true;
        frame.havePpq = true;
        frame.ppqAtBlockStart = 0.0;
        frame.bpm = 120.0;
        s.process(midi, seq, frame, blockSize, sampleRate, 1, 36, 1234);

        require(countNoteOns(midi) == 1, "PPQ: one boundary inside first block");
        require(countNoteOffs(midi) == 0, "PPQ: gate 0.5 extends past block");
        require(s.playHeadStep() == 0, "PPQ: playhead starts at step 0");

        // Enough blocks to wrap past the sequence end.
        int totalNotes = countNoteOns(midi);
        const double blocksPerQuarter =
            sampleRate / (120.0 / 60.0) / static_cast<double>(blockSize);
        double ppq = 1.0 / blocksPerQuarter;
        for (int b = 1; b < 64; ++b) {
            juce::MidiBuffer block;
            frame.ppqAtBlockStart = ppq;
            s.process(block, seq, frame, blockSize, sampleRate, 1, 36, 1234);
            totalNotes += countNoteOns(block);
            ppq += 1.0 / blocksPerQuarter;
        }
        require(totalNotes == 17, "PPQ: 17 steps over 64 blocks at 120 BPM");
    }

    // 2: Seek detection flushes and re-anchors (no stuck notes).
    {
        const auto seq = makeSequence();
        GeneratedNoteScheduler s;
        s.reset(sampleRate);
        s.reseedProbability(7);

        TransportFrame frame;
        frame.playing = true;
        frame.havePpq = true;
        frame.bpm = 120.0;
        frame.ppqAtBlockStart = 0.0;

        // Run a few blocks, then jump the PPQ forward by 8 quarters (seek).
        double ppq = 0.0;
        for (int b = 0; b < 8; ++b) {
            juce::MidiBuffer block;
            frame.ppqAtBlockStart = ppq;
            s.process(block, seq, frame, blockSize, sampleRate, 1, 36, 7);
            ppq += 1.0 / (sampleRate / 2.0 / static_cast<double>(blockSize));
        }
        juce::MidiBuffer seekBlock;
        frame.ppqAtBlockStart = ppq + 8.0; // big jump = seek
        s.process(seekBlock, seq, frame, blockSize, sampleRate, 1, 36, 7);

        // After a seek the previous note must be flushed (note-off present)
        // and the playhead must re-anchor to the new musical position.
        require(countNoteOffs(seekBlock) >= 1,
                "seek: sounding generated note flushed with note-off");
        require(s.playHeadStep() >= 0, "seek: playhead re-anchored");
    }

    // 3: Stop flushes; every note-on is matched by a note-off (no hang).
    {
        auto gated = makeSequence();
        for (int i = 0; i < 16; ++i)
            gated[i].gateWidth = 0.05f; // short gate: note-offs inside blocks

        GeneratedNoteScheduler s;
        s.reset(sampleRate);
        s.reseedProbability(42);

        TransportFrame frame;
        frame.playing = true;
        frame.havePpq = true;
        frame.ppqAtBlockStart = 0.0;
        frame.bpm = 120.0;

        juce::MidiBuffer all;
        double ppq = 0.0;
        const double quarterPerBlock =
            static_cast<double>(blockSize) / sampleRate * 2.0;
        for (int b = 0; b < 32; ++b) {
            juce::MidiBuffer block;
            frame.ppqAtBlockStart = ppq;
            s.process(block, gated, frame, blockSize, sampleRate, 3, 36, 42);
            for (const auto metadata : block)
                all.addEvent(metadata.getMessage(), metadata.samplePosition);
            ppq += quarterPerBlock;
        }

        // Transport stop: flush must cut the sounding note.
        frame.playing = false;
        juce::MidiBuffer stopBlock;
        s.process(stopBlock, gated, frame, blockSize, sampleRate, 3, 36, 42);
        require(s.playHeadStep() == -1, "stop: playhead cleared");

        for (const auto metadata : all)
            stopBlock.addEvent(metadata.getMessage(), metadata.samplePosition);

        require(countNoteOns(stopBlock) > 0, "stop: session produced notes");
        require(countNoteOns(stopBlock) == countNoteOffs(stopBlock),
                "stop: every note-on matched by note-off (no stuck notes)");
    }

    // 4: Fallback path (no PPQ) keeps the documented free-running behavior.
    {
        const auto seq = makeSequence();
        GeneratedNoteScheduler s;
        s.reset(sampleRate);
        s.reseedProbability(99);

        TransportFrame frame;
        frame.playing = true;
        frame.havePpq = false;
        frame.bpm = 120.0;

        // First note fires at offset 0 of the first block
        // (samplesUntilNextStep == 0 after reset).
        juce::MidiBuffer midi;
        s.process(midi, seq, frame, blockSize, sampleRate, 1, 36, 99);
        require(countNoteOns(midi) == 1, "fallback: first note at start");
        require(s.playHeadStep() == 0, "fallback: playhead at step 0");
    }

    // 5: Determinism: same seed + same PPQ path -> identical MIDI stream.
    {
        const auto seq = makeSequence();

        // Collect (sample offset, raw message bytes) for each build.
        std::vector<std::pair<int, std::vector<std::uint8_t>>> runs;
        for (int variant = 0; variant < 2; ++variant) {
            GeneratedNoteScheduler s;
            s.reset(sampleRate);
            s.reseedProbability(31337);
            TransportFrame frame;
            frame.playing = true;
            frame.havePpq = true;
            frame.bpm = 140.0;
            double ppq = 0.0;
            const double quarterPerBlock =
                static_cast<double>(blockSize) / sampleRate * (140.0 / 60.0);
            for (int blk = 0; blk < 16; ++blk) {
                juce::MidiBuffer block;
                frame.ppqAtBlockStart = ppq;
                s.process(block, seq, frame, blockSize, sampleRate, 2, 36,
                          31337);
                for (const auto metadata : block) {
                    const auto& msg = metadata.getMessage();
                    runs.emplace_back(
                        metadata.samplePosition,
                        std::vector<std::uint8_t>(
                            msg.getRawData(),
                            msg.getRawData() + msg.getRawDataSize()));
                }
                ppq += quarterPerBlock;
            }
        }

        const auto half = static_cast<int>(runs.size()) / 2;
        require(static_cast<int>(runs.size()) == 2 * half,
                "determinism: even number of events across two runs");
        for (int i = 0; i < half; ++i) {
            require(runs[static_cast<std::size_t>(i)].first
                        == runs[static_cast<std::size_t>(i + half)].first,
                    "determinism: identical sample offsets");
            require(runs[static_cast<std::size_t>(i)].second
                        == runs[static_cast<std::size_t>(i + half)].second,
                    "determinism: identical message bytes");
        }
    }

    std::cout << "GeneratedNoteScheduler tests passed (" << testsPassed << "/"
              << testsRun << ")\n";
    return EXIT_SUCCESS;
}
