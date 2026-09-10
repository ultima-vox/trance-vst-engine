#include "parts/PartRouter.h"
#include "parts/PartMidiDelay.h"
#include <cstdlib>
#include <iostream>

namespace {
int checks {};
#define REQUIRE(condition, message) do { ++checks; if (!(condition)) { std::cerr << "FAIL: " << message << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (false)

int channelOfFirst(const juce::MidiBuffer& buffer)
{
    return buffer.isEmpty() ? 0 : (*buffer.begin()).getMessage().getChannel();
}
}

int main()
{
    using namespace vstengine::parts;
    PartRegistry registry;
    PartRouter router;
    PartMidiBuffers routed;
    routed.prepare(4096);
    juce::MidiBuffer input;

    input.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8) 100), 0);
    router.route(input, registry, routed);
    REQUIRE(routed[0].getNumEvents() == 1, "CH1 routes to Bass");
    REQUIRE(routed[1].isEmpty(), "CH1 does not route to Kick");

    input.clear();
    input.addEvent(juce::MidiMessage::noteOn(2, 48, (juce::uint8) 100), 0);
    router.route(input, registry, routed);
    REQUIRE(routed[0].isEmpty(), "CH2 does not route to Bass");
    REQUIRE(routed[1].getNumEvents() == 1, "CH2 routes to Kick");

    input.clear();
    input.addEvent(juce::MidiMessage::noteOn(3, 60, (juce::uint8) 100), 0);
    router.route(input, registry, routed);
    REQUIRE(routed[0].isEmpty() && routed[1].isEmpty(),
            "unregistered CH3 routes nowhere");

    registry.find(PartId::bass)->midiChannel = 5;
    registry.find(PartId::kick)->midiChannel = 9;
    input.clear();
    input.addEvent(juce::MidiMessage::noteOn(5, 36, (juce::uint8) 100), 2);
    input.addEvent(juce::MidiMessage::noteOn(9, 48, (juce::uint8) 100), 3);
    router.route(input, registry, routed);
    REQUIRE(routed[0].getNumEvents() == 1 && channelOfFirst(routed[0]) == 5,
            "changed Bass channel routes");
    REQUIRE(routed[1].getNumEvents() == 1 && channelOfFirst(routed[1]) == 9,
            "changed Kick channel routes");

    routed.clear();
    const auto audition = juce::MidiMessage::noteOn(1, 40, (juce::uint8) 127);
    REQUIRE(router.routeToPart(audition, 7, PartId::kick, registry, routed),
            "Kick audition destination exists");
    REQUIRE(routed[0].isEmpty(), "Kick audition does not enter Bass");
    REQUIRE(routed[1].getNumEvents() == 1 && channelOfFirst(routed[1]) == 9,
            "Kick audition rewrites destination channel");

    registry.find(PartId::kick)->enabled = false;
    routed.clear();
    REQUIRE(!router.routeToPart(audition, 0, PartId::kick, registry, routed),
            "disabled Part rejects audition");

    PartMidiDelay delay;
    juce::MidiBuffer delayed;
    input.clear();
    input.addEvent(juce::MidiMessage::noteOn(5, 36, (juce::uint8) 100), 120);
    delay.process(input, delayed, 128, 20);
    REQUIRE(delayed.isEmpty(), "Part delay holds cross-block note");
    input.clear();
    delay.process(input, delayed, 128, 20);
    REQUIRE(delayed.getNumEvents() == 1
                && (*delayed.begin()).samplePosition == 12,
            "Part delay preserves cross-block sample offset");

    input.clear();
    input.addEvent(juce::MidiMessage::noteOn(5, 40, (juce::uint8) 100), 120);
    delay.process(input, delayed, 128, 20);
    REQUIRE(delayed.isEmpty(), "Part delay queues note before panic");
    input.clear();
    input.addEvent(juce::MidiMessage::controllerEvent(5, 123, 0), 7);
    delay.process(input, delayed, 128, 20);
    REQUIRE(delayed.getNumEvents() == 1
                && (*delayed.begin()).samplePosition == 7,
            "Part panic controller is never delayed");
    input.clear();
    delay.process(input, delayed, 128, 20);
    REQUIRE(delayed.isEmpty(), "Part panic clears pending delayed notes");

    std::cout << "PartRouter tests passed (" << checks << ")\n";
    return EXIT_SUCCESS;
}
