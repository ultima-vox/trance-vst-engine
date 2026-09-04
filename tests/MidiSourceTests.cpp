#include "midi/SourceSelector.h"
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

void requireDecision(vstengine::midi::SourceMode mode,
                     bool external, bool gui, bool expected,
                     const char* label)
{
    require(vstengine::midi::shouldRunGenerator(mode, external, gui) == expected,
            label);
}

} // namespace

int main()
{
    using vstengine::midi::SourceMode;

    // --- AUTO: external/GUI input wins; generator runs only with no input ---
    requireDecision(SourceMode::autoDetect, false, false, true,
                    "AUTO: no input -> generator runs");
    requireDecision(SourceMode::autoDetect, true, false, false,
                    "AUTO: host notes -> external wins");
    requireDecision(SourceMode::autoDetect, false, true, false,
                    "AUTO: GUI notes -> external wins");
    requireDecision(SourceMode::autoDetect, true, true, false,
                    "AUTO: both -> external wins");

    // --- PIANO ROLL: incoming host / GUI only --------------------------------
    requireDecision(SourceMode::pianoRoll, false, false, false,
                    "PIANO ROLL: no input -> generator never runs");
    requireDecision(SourceMode::pianoRoll, true, false, false,
                    "PIANO ROLL: host notes -> generator never runs");
    requireDecision(SourceMode::pianoRoll, false, true, false,
                    "PIANO ROLL: GUI notes -> generator never runs");
    requireDecision(SourceMode::pianoRoll, true, true, false,
                    "PIANO ROLL: both -> generator never runs");

    // --- GENERATOR: only internal generated material --------------------------
    requireDecision(SourceMode::generator, false, false, true,
                    "GENERATOR: no input -> generator runs");
    requireDecision(SourceMode::generator, true, false, true,
                    "GENERATOR: host notes dropped, generator runs");
    requireDecision(SourceMode::generator, false, true, true,
                    "GENERATOR: GUI notes do not stop the generator");
    requireDecision(SourceMode::generator, true, true, true,
                    "GENERATOR: both present -> generator runs");

    // --- BOTH: input and generated material combined --------------------------
    requireDecision(SourceMode::both, false, false, true,
                    "BOTH: no input -> generator runs");
    requireDecision(SourceMode::both, true, false, true,
                    "BOTH: host notes -> generator still runs");
    requireDecision(SourceMode::both, false, true, true,
                    "BOTH: GUI notes -> generator still runs");
    requireDecision(SourceMode::both, true, true, true,
                    "BOTH: both present -> generator runs");

    // --- Generated MIDI can never act as user input ----------------------------
    // The selector sees ONLY external host notes and GUI notes. "Generated
    // notes present" is deliberately not an input to the decision, so the
    // generator can never be suppressed by its own output — the structural
    // regression this module exists to prevent.
    require(vstengine::midi::shouldRunGenerator(
                SourceMode::autoDetect, true, false) == false,
            "host notes suppress generator in AUTO");

    std::cout << "MidiSource tests passed (" << testsPassed << "/"
              << testsRun << ")\n";
    return EXIT_SUCCESS;
}