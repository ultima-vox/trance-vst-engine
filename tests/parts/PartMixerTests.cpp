#include "parts/PartMixer.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int checks {};
#define REQUIRE(condition, message) do { ++checks; if (!(condition)) { std::cerr << "FAIL: " << message << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (false)

bool near(float actual, float expected)
{
    return std::abs(actual - expected) < 1.0e-6f;
}
}

int main()
{
    using namespace vstengine::parts;
    PartRegistry registry;
    juce::AudioBuffer<float> bass(2, 8), kick(2, 8), output(2, 8);
    bass.clear();
    kick.clear();
    for (int channel = 0; channel < 2; ++channel) {
        bass.addFrom(channel, 0, std::array<float, 8> { 1,1,1,1,1,1,1,1 }.data(), 8);
        kick.addFrom(channel, 0, std::array<float, 8> { 2,2,2,2,2,2,2,2 }.data(), 8);
    }
    PartMixer::Inputs inputs { &bass, &kick };

    output.clear();
    PartMixer::mix(registry, inputs, output, 8);
    REQUIRE(near(output.getSample(0, 0), 3.0f)
                && near(output.getSample(1, 0), 3.0f),
            "enabled Parts sum independently");

    registry.find(PartId::bass)->mute = true;
    output.clear();
    PartMixer::mix(registry, inputs, output, 8);
    REQUIRE(near(output.getSample(0, 0), 2.0f), "Bass mute leaves Kick");

    registry.find(PartId::bass)->mute = false;
    registry.find(PartId::bass)->solo = true;
    output.clear();
    PartMixer::mix(registry, inputs, output, 8);
    REQUIRE(near(output.getSample(0, 0), 1.0f), "Bass solo suppresses Kick");

    registry.find(PartId::bass)->solo = false;
    registry.find(PartId::kick)->solo = true;
    output.clear();
    PartMixer::mix(registry, inputs, output, 8);
    REQUIRE(near(output.getSample(0, 0), 2.0f), "Kick solo suppresses Bass");

    registry.find(PartId::kick)->solo = false;
    registry.find(PartId::kick)->mute = true;
    auto* bassPart = registry.find(PartId::bass);
    bassPart->level = 0.5f;
    bassPart->pan = 1.0f;
    output.clear();
    PartMixer::mix(registry, inputs, output, 8);
    REQUIRE(near(output.getSample(0, 0), 0.0f), "hard-right pan silences left");
    REQUIRE(near(output.getSample(1, 0), 0.5f), "Part level applies to right");

    bassPart->enabled = false;
    output.clear();
    PartMixer::mix(registry, inputs, output, 8);
    REQUIRE(near(output.getSample(0, 0), 0.0f)
                && near(output.getSample(1, 0), 0.0f),
            "disabled and muted Parts stay silent");

    std::cout << "PartMixer tests passed (" << checks << ")\n";
    return EXIT_SUCCESS;
}
