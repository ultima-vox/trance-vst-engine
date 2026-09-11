#include "modules/BuiltInProvider.h"
#include "rack/Rack.h"
#include "rack/RackState.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>

namespace {
int run = 0;
#define REQUIRE(c, m) do { ++run; if (!(c)) { std::cerr << "FAILED: " << m \
    << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (false)
VoxMidiEventV1 noteOn(int channel, int note)
{
    return { 0, 3, { static_cast<std::uint8_t>(0x90 | (channel - 1)),
                      static_cast<std::uint8_t>(note), 110 } };
}
double energy(const std::vector<float>& audio)
{
    return std::inner_product(audio.begin(), audio.end(), audio.begin(), 0.0);
}
std::vector<float> render(vstengine::rack::Rack& rack,
                          std::span<const VoxMidiEventV1> midi)
{
    std::vector<float> left(2048), right(2048);
    std::array<float*, 2> outputs { left.data(), right.data() };
    rack.process(outputs, static_cast<std::uint32_t>(left.size()), midi,
                 145.0, 0.0, true);
    return left;
}
} // namespace

int main()
{
    using namespace vstengine;
    instrument::InstrumentRegistry registry;
    std::string diagnostic;
    REQUIRE(registry.registerProvider(modules::createBuiltInProvider(), diagnostic),
            "built-in provider registers");
    REQUIRE(registry.descriptors().size() == 2,
            "Bass and independent reference module registered");

    rack::Rack rack(registry);
    REQUIRE(rack.prepare({ 48000.0, 2048, 2 }), "rack prepares");
    auto state = rack.state();
    state[0].instrumentId = modules::bassInstrumentId;
    state[0].routing = { rack::RouteMode::channel, 1, 0, 127, 1, 127, 0 };
    state[0].macros = { 0.45f, 0.6f, 0.5f, 0.25f, 0.35f, 0.5f, 0.0f, 0.1f };
    state[1].instrumentId = modules::referenceInstrumentId;
    state[1].routing = { rack::RouteMode::channel, 2, 0, 127, 1, 127, 0 };
    state[1].macros[0] = 0.5f;
    state[1].macros[1] = 0.7f;
    REQUIRE(rack.replaceState(state, nullptr, diagnostic),
            "rack state loads transactionally");
    REQUIRE(rack.descriptor(0)->id == modules::bassInstrumentId
                && rack.descriptor(1)->id == modules::referenceInstrumentId,
            "slots resolve through descriptor boundary");

    const std::array bassMidi { noteOn(1, 36) };
    rack.reset();
    const auto bass = render(rack, bassMidi);
    REQUIRE(energy(bass) > 1.0e-5, "CH1 renders Bass module");
    const std::array referenceMidi { noteOn(2, 60) };
    rack.reset();
    const auto reference = render(rack, referenceMidi);
    REQUIRE(energy(reference) > 1.0e-5, "CH2 renders second real module");
    double difference = 0.0;
    for (std::size_t i = 0; i < bass.size(); ++i)
        difference += std::abs(bass[i] - reference[i]);
    REQUIRE(difference > 1.0, "independent modules produce different renders");

    const std::array wrongMidi { noteOn(3, 48) };
    rack.reset();
    REQUIRE(energy(render(rack, wrongMidi)) < 1.0e-12,
            "wrong channel reaches no engine");

    state[1].routing = { rack::RouteMode::layer, 1, 48, 72, 80, 127, 0 };
    REQUIRE(rack.replaceState(state, nullptr, diagnostic), "Layer state loads");
    rack.reset();
    const std::array layerMidi { noteOn(1, 60) };
    REQUIRE(energy(render(rack, layerMidi)) > energy(reference) * 0.1,
            "explicit Layer adds second module");

    const auto beforeInvalid = rack.state()[0].instrumentId;
    auto invalid = rack.state();
    invalid[0].routing.keyLow = 100;
    invalid[0].routing.keyHigh = 20;
    REQUIRE(!rack.replaceState(invalid, nullptr, diagnostic),
            "invalid state rejected");
    REQUIRE(rack.state()[0].instrumentId == beforeInvalid,
            "failed state load leaves live rack untouched");

    REQUIRE(rack.loadModule(1, "com.ultimavox.missing", nullptr, diagnostic),
            "missing module commits as unresolved state");
    REQUIRE(rack.runtimeState()[1].resolution
                == instrument::ResolutionStatus::missingModule,
            "missing module status explicit");
    REQUIRE(rack.state()[1].instrumentId == "com.ultimavox.missing",
            "missing InstrumentId preserved without fallback");
    const auto serialized = rack::state::serialize(rack.state());
    std::array<rack::PersistentSlotState, instrument::maxSlots> decoded;
    REQUIRE(rack::state::deserialize(serialized, decoded, diagnostic),
            "canonical rack state parses");
    REQUIRE(rack::state::serialize(decoded).toXmlString()
                == serialized.toXmlString(),
            "save-load-save serialization canonical");
    REQUIRE(decoded[1].instrumentId == "com.ultimavox.missing",
            "unresolved identity survives serialization");

    auto malformed = serialized.createCopy();
    malformed.getChild(0).setProperty("instrumentId", "Invalid/Instrument", nullptr);
    REQUIRE(!rack::state::deserialize(malformed, decoded, diagnostic),
            "invalid persisted InstrumentId rejected");
    malformed = serialized.createCopy();
    malformed.getChild(0).setProperty("macroMap1", "Invalid Parameter", nullptr);
    REQUIRE(!rack::state::deserialize(malformed, decoded, diagnostic),
            "invalid persisted macro ParameterId rejected");
    malformed = serialized.createCopy();
    malformed.getChild(0).setProperty("instrumentVersion", "4294967296", nullptr);
    REQUIRE(!rack::state::deserialize(malformed, decoded, diagnostic),
            "overflowed uint32 metadata rejected");
    malformed = serialized.createCopy();
    malformed.getChild(0).setProperty("contentVersion", -1, nullptr);
    REQUIRE(!rack::state::deserialize(malformed, decoded, diagnostic),
            "negative unsigned metadata rejected");
    malformed = serialized.createCopy();
    const auto tooLargeBase64 = juce::String::repeatedString(
        "A", static_cast<int>(4u * ((rack::state::maxModulePayloadBytes + 2u)
                                    / 3u) + 1u));
    malformed.getChild(1).setProperty("modulePayload", tooLargeBase64, nullptr);
    REQUIRE(!rack::state::deserialize(malformed, decoded, diagnostic),
            "oversized unresolved module payload rejected before decode");

    auto nonCanonical = serialized.createCopy();
    nonCanonical.setProperty("unknownRootField", 99, nullptr);
    nonCanonical.getChild(0).setProperty("unknownSlotField", "ignored", nullptr);
    REQUIRE(rack::state::deserialize(nonCanonical, decoded, diagnostic),
            "unknown fields parse without changing known state");
    REQUIRE(rack::state::serialize(decoded).toXmlString()
                == serialized.toXmlString(),
            "canonical serialization strips unknown fields");
    rack.reset();
    REQUIRE(energy(render(rack, bassMidi)) > 1.0e-5,
            "unresolved slot cannot leak into working Bass slot");

    rack.reset();
    const auto first = render(rack, bassMidi);
    rack.reset();
    const auto second = render(rack, bassMidi);
    REQUIRE(first == second, "rack reset render deterministic");
    REQUIRE(rack.latencySamples() == 0 && rack.tailSamples() > 0,
            "rack aggregates latency and tail");

    std::cout << "Rack integration tests passed (" << run << ")\n";
    return EXIT_SUCCESS;
}
