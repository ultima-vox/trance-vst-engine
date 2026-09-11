#include "rack/RackRouter.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {
int run = 0;
#define REQUIRE(c, m) do { ++run; if (!(c)) { std::cerr << "FAILED: " << m \
    << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (false)
VoxMidiEventV1 note(std::uint8_t status, std::uint8_t value,
                    std::uint8_t velocity = 100)
{
    return { 0, 3, { status, value, velocity } };
}
VoxMidiEventV1 control(std::uint8_t channel, std::uint8_t controller,
                       std::uint32_t sampleOffset = 0)
{
    return { sampleOffset, 3,
             { static_cast<std::uint8_t>(0xb0 | channel), controller, 0 } };
}
} // namespace

int main()
{
    using namespace vstengine;
    std::array<rack::ProcessSlotControls, instrument::maxSlots> slots {};
    slots[0].slotId = instrument::initialSlotId(0);
    slots[0].routing = { rack::RouteMode::channel, 1, 36, 60, 50, 127, 12 };
    slots[1].slotId = instrument::initialSlotId(1);
    slots[1].routing = { rack::RouteMode::layer, 1, 48, 72, 1, 127, -12 };
    slots[2].slotId = instrument::initialSlotId(2);
    slots[2].routing = { rack::RouteMode::off, 2, 0, 127, 1, 127, 0 };

    rack::RackRouter router;
    std::array<rack::RackRouter::DestinationBuffer, instrument::maxSlots> output;
    const std::array input { note(0x90, 48, 100) };
    router.route(input, slots, output);
    REQUIRE(output[0].size == 1 && output[0].events[0].data[1] == 60,
            "CH1 zone routes and transposes Bass");
    REQUIRE(output[1].size == 1 && output[1].events[0].data[1] == 36,
            "explicit Layer routes independently");
    REQUIRE(output[2].size == 0, "OFF slot receives nothing");

    const std::array wrongChannel { note(0x91, 40, 100) };
    router.route(wrongChannel, slots, output);
    REQUIRE(output[0].size == 0, "wrong channel blocked");
    REQUIRE(output[1].size == 0, "Layer key zone still enforced");

    router.reset();
    const std::array lowVelocity { note(0x90, 48, 20) };
    router.route(lowVelocity, slots, output);
    REQUIRE(output[0].size == 0 && output[1].size == 1,
            "velocity zones isolate overlapping slots");

    router.route(input, slots, output);
    std::swap(slots[0], slots[1]);
    slots[1].routing.mode = rack::RouteMode::off;
    const std::array off { note(0x80, 48, 0) };
    router.route(off, slots, output);
    REQUIRE(output[0].size == 1 && output[0].events[0].data[1] == 36,
            "NoteOff follows Layer SlotId after reorder");
    REQUIRE(output[1].size == 1 && output[1].events[0].data[1] == 60,
            "NoteOff follows Bass SlotId after route disabled");

    router.reset();
    router.route(off, slots, output);
    REQUIRE(output[0].size == 0 && output[1].size == 0,
            "reset clears note ownership");

    for (const std::uint8_t controller : { std::uint8_t { 120 },
                                           std::uint8_t { 123 } }) {
        std::array<rack::ProcessSlotControls, instrument::maxSlots> panicSlots {};
        panicSlots[0].slotId = instrument::initialSlotId(0);
        panicSlots[0].routing = { rack::RouteMode::channel, 1 };
        panicSlots[1].slotId = instrument::initialSlotId(1);
        panicSlots[1].routing = { rack::RouteMode::layer, 1 };
        panicSlots[2].slotId = instrument::initialSlotId(2);
        panicSlots[2].routing = { rack::RouteMode::off, 1 };
        panicSlots[3].slotId = instrument::initialSlotId(3);
        panicSlots[3].routing = { rack::RouteMode::channel, 2 };

        auto panicRouter = std::make_unique<rack::RackRouter>();
        auto panicOutput = std::make_unique<std::array<
            rack::RackRouter::DestinationBuffer, instrument::maxSlots>>();
        const std::array held { note(0x90, 52) };
        panicRouter->route(held, panicSlots, *panicOutput);
        panicSlots[1].routing.channel = 2; // Owner must survive live reroute.
        panicSlots[2].routing.mode = rack::RouteMode::channel;
        const std::array panic { control(0, controller, 17) };
        panicRouter->route(panic, panicSlots, *panicOutput);
        REQUIRE((*panicOutput)[0].size == 1
                    && (*panicOutput)[0].events[0].sampleOffset == 17,
                "channel panic reaches current target once");
        REQUIRE((*panicOutput)[1].size == 1,
                "channel panic reaches prior SlotId owner after reroute");
        REQUIRE((*panicOutput)[2].size == 1,
                "channel panic reaches newly assigned current target");
        REQUIRE((*panicOutput)[3].size == 0,
                "channel panic cannot affect unrelated channel");
        panicRouter->route(std::array { note(0x80, 52, 0) }, panicSlots,
                           *panicOutput);
        REQUIRE((*panicOutput)[0].size == 0 && (*panicOutput)[1].size == 0,
                "channel panic clears source-channel ownership");
    }

    {
        std::array<rack::ProcessSlotControls, instrument::maxSlots> edgeSlots {};
        edgeSlots[0].slotId = instrument::initialSlotId(0);
        edgeSlots[0].routing = { rack::RouteMode::channel, 1 };
        auto edgeRouter = std::make_unique<rack::RackRouter>();
        auto edgeOutput = std::make_unique<std::array<
            rack::RackRouter::DestinationBuffer, instrument::maxSlots>>();
        auto first = note(0x90, 40);
        first.sampleOffset = 0;
        auto last = note(0x90, 41);
        last.sampleOffset = 511;
        edgeRouter->route(std::array { first, last }, edgeSlots, *edgeOutput);
        REQUIRE((*edgeOutput)[0].size == 2
                    && (*edgeOutput)[0].events[0].sampleOffset == 0
                    && (*edgeOutput)[0].events[1].sampleOffset == 511,
                "block-edge MIDI offsets preserved exactly");
    }

    {
        std::array<rack::ProcessSlotControls, instrument::maxSlots> fullSlots {};
        fullSlots[0].slotId = instrument::initialSlotId(0);
        fullSlots[0].routing = { rack::RouteMode::channel, 1 };
        auto fullRouter = std::make_unique<rack::RackRouter>();
        auto fullOutput = std::make_unique<std::array<
            rack::RackRouter::DestinationBuffer, instrument::maxSlots>>();
        std::vector<VoxMidiEventV1> events(
            rack::RackRouter::eventCapacity, control(0, 1));
        events.push_back(note(0x90, 60));
        fullRouter->route(events, fullSlots, *fullOutput);
        REQUIRE((*fullOutput)[0].size == rack::RackRouter::eventCapacity
                    && (*fullOutput)[0].dropped == 1,
                "full destination reports deterministic overflow");
        fullRouter->route(std::array { note(0x80, 60, 0) }, fullSlots,
                          *fullOutput);
        REQUIRE((*fullOutput)[0].size == 0,
                "dropped NoteOn never commits phantom ownership");

        for (std::size_t i = 0; i < rack::RackRouter::eventCapacity; ++i)
            (*fullOutput)[0].push(control(0, 1));
        fullRouter->routeAudition(std::array { note(0x90, 61) },
                                  fullSlots[0].slotId, fullSlots, *fullOutput);
        for (auto& destination : *fullOutput) destination.clear();
        fullRouter->routeAudition(std::array { note(0x80, 61, 0) },
                                  fullSlots[0].slotId, fullSlots, *fullOutput);
        REQUIRE((*fullOutput)[0].size == 0,
                "dropped audition NoteOn never commits phantom ownership");
    }
    std::cout << "Rack router tests passed (" << run << ")\n";
    return EXIT_SUCCESS;
}
