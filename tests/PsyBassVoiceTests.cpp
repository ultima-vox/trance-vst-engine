#include "dsp/PsyBassVoice.h"
#include <cmath>
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

int main()
{
    using namespace vstengine::dsp;

    // A Synthesiser is required to manage voice state (currentlyPlayingNote
    // is private). We use a single-voice synthesiser to prove deterministic
    // glide: consecutive notes always land on the same voice.
    juce::Synthesiser synth;
    synth.setCurrentPlaybackSampleRate(44100.0);
    synth.addSound(new PsyBassSound());
    auto* voice = new PsyBassVoice();
    synth.addVoice(voice);

    // 1: Deterministic glide. Two adjacent notes with slide enabled glide on
    //    the same voice. Monophonic setup guarantees the voice is active when
    //    the second note arrives.
    {
        // Clear any prior state
        synth.allNotesOff(0, false);

        // Request glide to MIDI 62 before the note-on
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<PsyBassVoice*>(synth.getVoice(i)))
                v->clearPendingGlide();

        // Start first note (MIDI 60)
        synth.noteOn(1, 60, 1.0f);
        require(voice->isVoiceActive(), "voice is active after first note");

        // Request glide to MIDI 62
        voice->requestGlide(62, 0.05f);

        // Start second note while first is still sounding (legato)
        synth.noteOn(1, 62, 1.0f);
        require(voice->isVoiceActive(), "voice is active after second note");
        require(voice->isGliding(), "glide is active after slide request on active voice");
    }

    // 2: No glide without a slide request (ordinary note-on).
    {
        synth.allNotesOff(0, false);
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<PsyBassVoice*>(synth.getVoice(i)))
                v->clearPendingGlide();

        synth.noteOn(1, 60, 1.0f);
        synth.noteOn(1, 62, 1.0f); // no requestGlide
        require(!voice->isGliding(), "no glide without a slide request");
    }

    // 3: Glide is deterministic — same inputs always produce the same state.
    {
        synth.allNotesOff(0, false);
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<PsyBassVoice*>(synth.getVoice(i)))
                v->clearPendingGlide();

        synth.noteOn(1, 48, 1.0f);
        voice->requestGlide(72, 0.1f);
        synth.noteOn(1, 72, 1.0f);
        require(voice->isGliding(), "deterministic: large-interval glide is active");

        // Rendering continues without getting stuck.
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        voice->renderNextBlock(buffer, 0, 512);
        require(voice->isVoiceActive(), "voice remains active after render");
    }

    std::cout << "PsyBassVoice glide tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}