#include "MidiExport.h"

namespace vstengine::midiexport {

namespace {

// Emits a deterministic pitch-bend ramp from the previous note's pitch into
// the target note, ending at bend center (0) exactly at noteOnTick.
void addSlideRamp(juce::MidiMessageSequence& track, int channel,
                  int previousNote, int targetNote,
                  double rampStartTick, double noteOnTick)
{
    const auto rampTicks = noteOnTick - rampStartTick;
    if (rampTicks <= 0.0)
        return;

    // Pitch bend offset that makes the sounding pitch equal the PREVIOUS
    // note's pitch: negative when the target note is higher.
    const auto semitoneDistance = static_cast<double>(previousNote - targetNote);
    const auto startOffset = juce::jlimit(
        -8191, 8191,
        juce::roundToInt((semitoneDistance / pitchBendRangeSemitones) * 8192.0));

    if (startOffset == 0)
        return; // same pitch: nothing to glide

    for (int segment = 0; segment <= slideRampSegments; ++segment) {
        const auto t = static_cast<double>(segment)
            / static_cast<double>(slideRampSegments);
        // Absolute 14-bit position: 8192 = centered (the target note's pitch).
        const auto position = juce::jlimit(
            0, 16383,
            8192 + juce::roundToInt(
                       static_cast<double>(startOffset) * (1.0 - t)));
        const auto tick = juce::jlimit(
            0.0, noteOnTick, rampStartTick + t * rampTicks);
        auto bendMessage = juce::MidiMessage::pitchWheel(channel, position);
        bendMessage.setTimeStamp(tick);
        track.addEvent(bendMessage);
    }
}

} // anonymous namespace

juce::MidiMessageSequence buildSequenceTrack(const vstengine::sequence::Sequence& seq,
                                             const Options& options)
{
    // Canonical timing: identical formula to realtime playback.
    const auto ticksPerStep =
        options.ticksPerQuarter / stepsPerQuarterNote(seq.getTimingMode());

    juce::MidiMessageSequence track;
    int previousAudibleNote = -1;
    vstengine::sequence::ProbabilityState probState { options.seed };

    for (int i = 0; i < seq.size(); ++i) {
        const auto& step = seq[i];

        // Advance RNG once per step regardless of gate state, then evaluate
        // probability. This rule is identical to realtime playback so the same
        // seed + sequence produces the same pass/skip decisions in both paths.
        const bool passesProbability = vstengine::sequence::shouldPlayStep(probState, step.probability);

        if (!step.gate)
            continue;

        if (!passesProbability)
            continue;

        const auto note = juce::jlimit(0, 127, options.rootNote + step.noteOffset);
        const auto velocity = step.accent ? 0.95f : juce::jlimit(0.0f, 1.0f, step.velocity);
        const auto stepStartTick = static_cast<double>(i) * ticksPerStep;

        const int ratchets = juce::jlimit(1, 8, step.ratchetCount);
        const auto subNoteTicks = ticksPerStep / static_cast<double>(ratchets);
        const auto gateWidth = juce::jlimit(0.0, 1.0, static_cast<double>(step.gateWidth));

        for (int r = 0; r < ratchets; ++r) {
            const auto noteOnTick = stepStartTick + r * subNoteTicks;
            const auto noteOffTick = noteOnTick + subNoteTicks * gateWidth;

            if (r == 0 && step.slideDuration > 0.0f
                && previousAudibleNote >= 0 && previousAudibleNote != note) {
                addSlideRamp(track, options.channel, previousAudibleNote, note,
                             noteOnTick - static_cast<double>(step.slideDuration) * ticksPerStep,
                             noteOnTick);
            }

            auto noteOn = juce::MidiMessage::noteOn(options.channel, note, velocity);
            noteOn.setTimeStamp(noteOnTick);
            track.addEvent(noteOn);

            auto noteOff = juce::MidiMessage::noteOff(options.channel, note);
            noteOff.setTimeStamp(noteOffTick);
            track.addEvent(noteOff);

            previousAudibleNote = note;
        }
    }

    auto endOfTrack = juce::MidiMessage::endOfTrack();
    endOfTrack.setTimeStamp(static_cast<double>(seq.size()) * ticksPerStep);
    track.addEvent(endOfTrack);
    track.updateMatchedPairs();

    return track;
}

} // namespace vstengine::midiexport