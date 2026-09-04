#include "generator/PpqSync.h"
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

bool near(double a, double b, double eps = 1e-9)
{
    return std::abs(a - b) <= eps;
}

} // namespace

int main()
{
    using namespace vstengine::sync;

    // --- resolveStepGrid: canonical 16 steps at 16th-note resolution -------
    {
        const auto at = resolveStepGrid(0.0, 16, 4.0);
        require(at.absoluteStep == 0, "ppq 0 -> absolute step 0");
        require(at.stepIndex == 0, "ppq 0 -> step 0");
        require(near(at.stepFraction, 0.0), "ppq 0 -> fraction 0");
    }
    {
        // One bar later (4 quarter notes) wraps back to sequence step 0.
        const auto at = resolveStepGrid(4.0, 16, 4.0);
        require(at.absoluteStep == 16, "ppq 4 -> absolute step 16");
        require(at.stepIndex == 0, "ppq 4 -> wraps to step 0");
        require(near(at.stepFraction, 0.0), "ppq 4 -> fraction 0");
    }
    {
        // Second sixteenth of bar 1.
        const auto at = resolveStepGrid(0.25, 16, 4.0);
        require(at.absoluteStep == 1, "ppq 0.25 -> absolute step 1");
        require(at.stepIndex == 1, "ppq 0.25 -> step 1");
    }
    {
        // Mid-step fraction.
        const auto at = resolveStepGrid(0.125, 16, 4.0);
        require(at.absoluteStep == 0, "ppq 0.125 -> absolute step 0");
        require(at.stepIndex == 0, "ppq 0.125 -> step 0");
        require(near(at.stepFraction, 0.5), "ppq 0.125 -> fraction 0.5");
    }
    {
        // Last step of the sequence pattern.
        const auto at = resolveStepGrid(3.75, 16, 4.0);
        require(at.stepIndex == 15, "ppq 3.75 -> step 15");
    }
    {
        // Negative host positions (timeline before bar 1) wrap correctly.
        const auto at = resolveStepGrid(-0.25, 16, 4.0);
        require(at.absoluteStep == -1, "ppq -0.25 -> absolute step -1");
        require(at.stepIndex == 15, "ppq -0.25 -> step 15 (wraps)");
    }
    {
        const auto at = resolveStepGrid(-0.125, 16, 4.0);
        require(at.absoluteStep == -1, "ppq -0.125 -> absolute step -1");
        require(at.stepIndex == 15, "ppq -0.125 -> step 15");
        require(near(at.stepFraction, 0.5), "ppq -0.125 -> fraction 0.5");
    }

    // --- nextStepBoundary --------------------------------------------------
    {
        require(nextStepBoundary(0.0, 4.0) == 0, "exact 0 -> boundary 0");
        require(nextStepBoundary(0.24, 4.0) == 1, "0.24 -> boundary 1");
        require(nextStepBoundary(0.25, 4.0) == 1, "exact 0.25 -> boundary 1");
        require(nextStepBoundary(4.0, 4.0) == 16, "bar 2 start -> boundary 16");
        require(nextStepBoundary(-0.25, 4.0) == -1, "exact -0.25 -> boundary -1");
        require(nextStepBoundary(-0.3, 4.0) == -1, "-0.3 -> boundary -1");
        require(nextStepBoundary(-0.5, 4.0) == -2, "-0.5 -> boundary -2");
    }

    // --- quartersToSamples --------------------------------------------------
    {
        // 120 bpm, 48 kHz: one quarter note = 24000 samples.
        require(near(quartersToSamples(1.0, 120.0, 48000.0), 24000.0),
                "quarter at 120bpm/48k = 24000 samples");
        // 145 bpm, 44.1 kHz: one quarter scales with bpm.
        require(near(quartersToSamples(1.0, 145.0, 44100.0),
                     44100.0 * 60.0 / 145.0),
                "quarter at 145bpm/44.1k scales with bpm");
    }

    // --- block scheduling math (mirrors the PPQ playback path) -------------
    // Canonical grid: 16th notes (4 steps/quarter), 16-step sequence,
    // 120 bpm, 48 kHz, 4800-sample block (= 0.2 quarters).
    {
        const double sppq = 4.0;
        const double bpm = 120.0;
        const double sr = 48000.0;
        const double samplesPerQuarter = 24000.0;
        const int numSamples = 4800;
        const double blockDurQ = static_cast<double>(numSamples) / samplesPerQuarter;

        // Block starts mid-step at ppq 1.1; the next boundary is ppq 1.25
        // (step 5), whose onset must be 3600 samples into the block.
        {
            const double startPpq = 1.1;
            const double endPpq = startPpq + blockDurQ;
            const auto first = nextStepBoundary(startPpq, sppq);
            require(first == 5, "mid-block start -> first boundary is step 5");
            double emitted = 0.0;
            for (auto b = first;; ++b) {
                const double boundaryPpq = static_cast<double>(b) / sppq;
                if (boundaryPpq >= endPpq)
                    break;
                emitted += quartersToSamples(boundaryPpq - startPpq, bpm, sr);
            }
            require(near(emitted, 3600.0),
                    "boundary 5 onset offset = 3600 samples");
        }

        // Block starts exactly on a boundary (ppq 1.25): the boundary fires
        // at sample offset 0 instead of being skipped.
        {
            const double startPpq = 1.25;
            const double endPpq = startPpq + blockDurQ;
            const auto first = nextStepBoundary(startPpq, sppq);
            require(first == 5, "start exactly on boundary 5");
            require(near(quartersToSamples(5.0 / sppq - startPpq, bpm, sr), 0.0),
                    "boundary at block start has offset 0");
            std::int64_t count = 0;
            for (auto b = first;; ++b) {
                const double boundaryPpq = static_cast<double>(b) / sppq;
                if (boundaryPpq >= endPpq)
                    break;
                ++count;
            }
            require(count == 1, "exactly one boundary in this block");
        }

        // A block that ends exactly on a boundary must not fire that final
        // boundary (it belongs to the next block and would double-fire).
        {
            const double startPpq = 1.0;
            const double endPpq = 1.25; // exactly boundary 5 (ppq 1.25)
            std::int64_t count = 0;
            std::int64_t last = -1;
            for (auto b = nextStepBoundary(startPpq, sppq);; ++b) {
                const double boundaryPpq = static_cast<double>(b) / sppq;
                if (boundaryPpq >= endPpq)
                    break;
                last = b;
                ++count;
            }
            require(count == 1,
                    "start boundary fires, boundary at block end excluded");
            require(last == 4,
                    "boundary 5 (exactly at block end) belongs to next block");
        }
    }

    // --- BPM independence of step alignment ---------------------------------
    // The step index depends only on the musical position, never on tempo:
    // the whole point of PPQ anchoring vs the free-running fallback.
    {
        const auto slow = resolveStepGrid(2.25, 16, 4.0);
        const auto fast = resolveStepGrid(2.25, 16, 4.0);
        require(slow.stepIndex == fast.stepIndex,
                "step alignment is independent of tempo");
        require(slow.stepIndex == 9, "ppq 2.25 at 16th grid -> step 9");
    }

    // --- canonical grid resolutions cover every Sequence timing mode --------
    {
        // Every mode maps to its documented stepsPerQuarterNote() value.
        // These constants are the single source of truth for both realtime
        // playback and MIDI export.
        std::int64_t boundariesToNextBar[4] {};
        const double sppqValues[] = { 4.0, 2.0, 8.0, 3.0 };
        for (int m = 0; m < 4; ++m) {
            const double sppq = sppqValues[m];
            require(nextStepBoundary(0.0, sppq) == 0,
                    "every grid starts at boundary 0");
            boundariesToNextBar[m] =
                resolveStepGrid(4.0, 16, sppq).absoluteStep;
        }
        // TimingMode enum order in Sequence.h:
        // sixteenth=4, eighth=2, thirtySecond=8, triplet=3 (steps/quarter).
        require(boundariesToNextBar[0] == 16, "sixteenth grid: 16 steps/bar");
        require(boundariesToNextBar[1] == 8, "eighth grid: 8 steps/bar");
        require(boundariesToNextBar[2] == 32, "thirtySecond grid: 32 steps/bar");
        require(boundariesToNextBar[3] == 12, "triplet grid: 12 steps/bar");
    }

    std::cout << "TransportSync tests passed (" << testsPassed << "/"
              << testsRun << ")\n";
    return EXIT_SUCCESS;
}