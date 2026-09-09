#include "midi/PartMidiRouter.h"
#include "kick/KickSynth.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int tests {};
#define require(c, m) do { ++tests; if (!(c)) { std::cerr << "FAIL: " << m << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (0)

void applyKickActions(const vstengine::midi::PartMidiRouter& router,
                      vstengine::kick::KickSynth& kick)
{
    for (int i = 0; i < router.numKickActions(); ++i) {
        const auto& action = router.kickAction(i);
        if (action.type
            == vstengine::midi::PartMidiRouter::KickActionType::trigger)
            kick.trigger(action.velocity, action.note, action.samplePosition);
        else
            kick.release(action.samplePosition);
    }
}

double energy(const juce::AudioBuffer<float>& buffer, int from = 0)
{
    double result = 0.0;
    for (int i = from; i < buffer.getNumSamples(); ++i) {
        const double sample = buffer.getSample(0, i);
        result += sample * sample;
    }
    return result;
}
}

int main()
{
    using vstengine::midi::PartMidiRouter;
    PartMidiRouter router;
    juce::MidiBuffer in, out;

    // Same-block bass delay; kick and unrelated channel stay undelayed.
    in.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100), 10);
    in.addEvent(juce::MidiMessage::noteOn(2, 60, (juce::uint8) 127), 12);
    in.addEvent(juce::MidiMessage::noteOn(3, 50, (juce::uint8) 90), 14);
    router.process(in, out, 128, 1, 2, 20);
    require(out.getNumEvents() == 2, "kick consumed; bass + other routed");
    auto it = out.begin();
    require((*it).samplePosition == 14 && (*it).getMessage().getChannel() == 3,
            "other channel unchanged");
    ++it;
    require((*it).samplePosition == 30 && (*it).getMessage().getChannel() == 1,
            "bass delayed inside block");
    require(router.numKickActions() == 1, "kick trigger emitted");
    require(router.kickAction(0).samplePosition == 12, "kick offset unchanged");

    // Ordinary kick note-off ignored; foreign panic does not release kick.
    in.clear();
    in.addEvent(juce::MidiMessage::noteOff(2, 60), 5);
    in.addEvent(juce::MidiMessage::controllerEvent(3, 123, 0), 6);
    router.process(in, out, 128, 1, 2, 20);
    require(router.numKickActions() == 0, "kick note-off ignored");
    require(out.getNumEvents() == 1, "foreign panic preserved");

    // Kick-channel CC120/123 use explicit panic path.
    in.clear();
    in.addEvent(juce::MidiMessage::controllerEvent(2, 120, 0), 7);
    router.process(in, out, 128, 1, 2, 20);
    require(router.numKickActions() == 1, "kick panic routed");
    require(router.kickAction(0).type == PartMidiRouter::KickActionType::panic,
            "panic action type");

    // End-to-end one-shot regression: ordinary kick note-off must not alter
    // tail. Compare against identical reference voice receiving no note-off.
    {
        vstengine::kick::KickParams params;
        params.pitchStart = 0.0f;
        params.bodyDecay = 1.0f;
        params.tail = 1.0f;
        params.click = 0.0f;
        params.sub = 0.0f;
        params.transient = 0.0f;
        params.phase = 90.0f;

        vstengine::kick::KickSynth routed, reference;
        routed.prepare(44100.0);
        reference.prepare(44100.0);
        routed.setParameters(params);
        reference.setParameters(params);
        juce::AudioBuffer<float> routedAudio(1, 512), referenceAudio(1, 512);

        router.reset();
        in.clear();
        in.addEvent(juce::MidiMessage::noteOn(
                        2, 60, static_cast<juce::uint8>(127)), 0);
        router.process(in, out, 512, 1, 2, 0);
        applyKickActions(router, routed);
        reference.trigger(1.0f, 60, 0);
        routedAudio.clear();
        referenceAudio.clear();
        routed.render(routedAudio, 512);
        reference.render(referenceAudio, 512);

        in.clear();
        in.addEvent(juce::MidiMessage::noteOff(2, 60), 0);
        router.process(in, out, 512, 1, 2, 0);
        applyKickActions(router, routed);
        require(router.numKickActions() == 0,
                "ordinary note-off creates no kick action");
        routedAudio.clear();
        referenceAudio.clear();
        routed.render(routedAudio, 512);
        reference.render(referenceAudio, 512);
        require(energy(routedAudio) > 1.0e-4,
                "ordinary note-off leaves audible tail");
        for (int i = 0; i < 512; ++i)
            require(std::abs(routedAudio.getSample(0, i)
                             - referenceAudio.getSample(0, i)) < 1.0e-6f,
                    "ordinary note-off does not change tail");
    }

    // Kick-channel panic follows explicit release path and stops long tail.
    {
        vstengine::kick::KickParams params;
        params.pitchStart = 0.0f;
        params.bodyDecay = 2.0f;
        params.tail = 1.0f;
        params.click = 0.0f;
        params.sub = 0.0f;
        params.transient = 0.0f;
        params.phase = 90.0f;
        vstengine::kick::KickSynth kick;
        kick.prepare(44100.0);
        kick.setParameters(params);
        juce::AudioBuffer<float> audio(1, 1024);

        router.reset();
        in.clear();
        in.addEvent(juce::MidiMessage::noteOn(
                        2, 60, static_cast<juce::uint8>(127)), 0);
        router.process(in, out, 128, 1, 2, 0);
        applyKickActions(router, kick);
        audio.clear();
        kick.render(audio, 128);
        require(kick.isActive(), "kick active before panic");

        in.clear();
        in.addEvent(juce::MidiMessage::controllerEvent(2, 123, 0), 0);
        router.process(in, out, 1024, 1, 2, 0);
        applyKickActions(router, kick);
        audio.clear();
        kick.render(audio, 1024);
        require(energy(audio, 256) < 1.0e-9,
                "kick panic fades to silence");
        require(!kick.isActive(), "kick inactive after panic");
    }

    // Cross-block sample accuracy.
    router.reset();
    in.clear();
    in.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100), 120);
    router.process(in, out, 128, 1, 2, 20);
    require(out.isEmpty(), "cross-block event held");
    in.clear();
    router.process(in, out, 128, 1, 2, 20);
    require(out.getNumEvents() == 1, "cross-block event emitted");
    require((*out.begin()).samplePosition == 12, "cross-block exact offset");

    // Bounded queue overflow: pending queue flushes without event loss.
    router.reset();
    in.clear();
    for (int i = 0; i < PartMidiRouter::maxPendingEvents + 1; ++i)
        in.addEvent(juce::MidiMessage::noteOn(1, i % 128, (juce::uint8) 100), 127);
    router.process(in, out, 128, 1, 2, 20);
    require(router.overflowCount() == 1, "overflow counted");
    require(out.getNumEvents() == PartMidiRouter::maxPendingEvents + 1,
            "overflow flush preserves all events");
    in.clear();
    router.process(in, out, 128, 1, 2, 20);
    require(out.isEmpty(), "overflow leaves no stale pending events");

    // Kick action overflow keeps panic even after trigger storm.
    router.reset();
    in.clear();
    for (int i = 0; i < PartMidiRouter::maxKickActionsPerBlock; ++i)
        in.addEvent(juce::MidiMessage::noteOn(
                        2, 60, static_cast<juce::uint8>(100)), i);
    in.addEvent(juce::MidiMessage::controllerEvent(2, 123, 0), 63);
    router.process(in, out, 128, 1, 2, 0);
    require(router.numKickActions() == PartMidiRouter::maxKickActionsPerBlock,
            "kick actions stay bounded");
    require(router.kickAction(PartMidiRouter::maxKickActionsPerBlock - 1).type
                == PartMidiRouter::KickActionType::panic,
            "panic survives kick action overflow");

    std::cout << "PartMidiRouter tests passed (" << tests << ")\n";
    return EXIT_SUCCESS;
}
