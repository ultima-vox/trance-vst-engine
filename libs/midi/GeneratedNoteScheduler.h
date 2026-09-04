#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "sequence/Sequence.h"
#include <array>
#include <cstdint>

namespace vstengine::midi {

// Deterministic generated-note scheduling for the psy-bass sequence.
//
// Owns the full generated-playback state machine that used to live in the
// plugin shell:
//   - PPQ-aligned scheduling when the host reports a musical position
//     (start locator, seek and cycle-loop all land on the correct step),
//   - the documented BPM-derived free-running fallback otherwise,
//   - probability rolls (one per step boundary, shared splitmix32 rule),
//   - ratchet sub-note tails carried across block borders,
//   - flush semantics so a generated note can never hang.
//
// Realtime contract (audio thread only):
//   - no allocation (fixed-capacity glide queue, events appended to the
//     caller's pre-existing MidiBuffer),
//   - no locks, no filesystem, no dynamic container growth,
//   - all per-block inputs are passed in; the scheduler never touches
//     APVTS, plugin, editor or host objects.
//
// Glide requests are emitted into a small fixed queue that the shell drains
// after process() and applies to the bass voices. Keeping this out-of-band
// (instead of virtual callbacks) avoids hidden virtual dispatch in the
// audio path. Capacity 64 exceeds any real block: at the slowest supported
// tempo (20 BPM clamped) and the densest grid (1/32) a 4096-sample block
// contains far fewer slide-eligible note onsets.

// Host transport observations for one audio block, bridged by the shell.
struct TransportFrame {
    bool playing { true };
    bool havePpq { false };
    double ppqAtBlockStart { 0.0 };
    double bpm { 145.0 };
};

// A legato-glide request for the bass voice (single fixed-size slot list).
struct GlideRequest {
    int noteNumber { -1 };
    float glideSeconds { 0.0f };
};

class GeneratedNoteScheduler final {
public:
    static constexpr int maxGlideRequests = 64;

    // Full reset of playback state (prepareToPlay / release paths).
    // Does NOT touch the probability seed: call reseedProbability() with the
    // current rngSeed value, exactly like the previous resetPlaybackState().
    void reset(double sampleRate) noexcept;

    void reseedProbability(std::uint32_t seed) noexcept;

    // Emits a note-off for any sounding generated note and resets all
    // generated-playback state. Called on stop, mode switches, seek/loop
    // realign and flush paths so generated notes can never get stuck.
    void flush(juce::MidiBuffer& midi, int offset) noexcept;

    // Schedules generated MIDI for one audio block. Dispatches to the
    // PPQ-aligned path or the free-running fallback depending on the frame.
    void process(juce::MidiBuffer& midi,
                 const vstengine::sequence::Sequence& sequence,
                 const TransportFrame& transport,
                 int numSamples,
                 double sampleRate,
                 int channel, int rootNote, std::uint32_t seed);

    // Canonical step containing the block-start musical position (PPQ path)
    // or the last scheduled step (fallback); -1 when nothing is playing.
    [[nodiscard]] int playHeadStep() const noexcept { return playHeadStep_; }

    // Glide request queue (drained by the shell once per block).
    [[nodiscard]] int numGlideRequests() const noexcept
    {
        return glideRequestCount;
    }
    [[nodiscard]] GlideRequest glideRequest(int index) const noexcept
    {
        return glideRequests[static_cast<std::size_t>(
            index < glideRequestCount ? index : glideRequestCount - 1)];
    }
    void clearGlideRequests() noexcept
    {
        glideRequestCount = 0;
        for (auto& request : glideRequests)
            request = GlideRequest {};
    }

private:
    void processPpq(juce::MidiBuffer& midi,
                    const vstengine::sequence::Sequence& sequence,
                    int numSamples, int channel, int rootNote,
                    double ppqAtBlockStart, double bpm, double sampleRate,
                    std::uint32_t seed);
    void processFallback(juce::MidiBuffer& midi,
                         const vstengine::sequence::Sequence& sequence,
                         int numSamples, int channel, int rootNote,
                         double bpm, double sampleRate,
                         std::uint32_t seed);
    void triggerNote(juce::MidiBuffer& midi, int offset, int channel,
                     int rootNote, const vstengine::sequence::Step& step,
                     double samplesPerStep, double subNoteSamples,
                     double sampleRate);
    void queueGlideRequest(int noteNumber, float glideSeconds) noexcept;

    // --- Generated-playback state (audio thread only, all preallocated) ---
    int currentStep {};
    int playHeadStep_ { -1 };
    int heldNote { -1 };
    int heldChannel { 1 };
    double samplesUntilNextStep {};
    double samplesUntilNoteOff { -1.0 };
    int ratchetsRemaining {};
    double samplesUntilNextRatchet {};
    double subNoteDuration {};
    int lastGeneratedNote { -1 };
    vstengine::sequence::ProbabilityState probabilityState {};
    std::uint64_t playedStepCounter {};
    bool transportWasPlaying { false };
    // Host-PPQ continuity state. continuityValid becomes true after the first
    // block whose end PPQ was observed; a PPQ position that does not continue
    // from the previous block's end is a seek or cycle-loop jump and forces a
    // hard realign of the generated cursor. The jump tolerance is computed per
    // block as half a step of the current canonical grid, which is far below
    // any real seek/loop distance yet far above host rounding / smooth
    // tempo-drift.
    bool continuityValid { false };
    double lastBlockEndPpq {};

    std::array<GlideRequest, maxGlideRequests> glideRequests {};
    int glideRequestCount { 0 };
};

} // namespace vstengine::midi
