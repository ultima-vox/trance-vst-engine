#include "parts/PartRegistry.h"
#include <cstdlib>
#include <iostream>

namespace {
int checks {};
#define REQUIRE(condition, message) do { ++checks; if (!(condition)) { std::cerr << "FAIL: " << message << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (false)
}

int main()
{
    using namespace vstengine::parts;
    PartRegistry registry;
    auto& bass = registry.find(PartId::bass)->sequence;
    auto& kick = registry.find(PartId::kick)->sequence;

    bass.setLength(16);
    kick.setLength(8);
    bass[0].gate = true;
    bass[0].noteOffset = 12;
    bass[0].slideDuration = 0.5f;
    kick[4].gate = true;
    kick[4].velocity = 0.6f;

    REQUIRE(bass.size() == 16 && kick.size() == 8,
            "Part sequence lengths independent");
    REQUIRE(!kick[0].gate && kick[0].noteOffset == 0,
            "Bass edits do not change Kick");
    REQUIRE(!bass[4].gate && bass[4].velocity == 1.0f,
            "Kick edits do not change Bass");

    bass.regenerateBySeed(111u);
    const auto bassCopy = bass.copy();
    kick.regenerateBySeed(222u);
    for (int i = 0; i < bass.size(); ++i) {
        REQUIRE(bass[i].gate == bassCopy[i].gate,
                "Kick generation leaves Bass gates unchanged");
        REQUIRE(bass[i].noteOffset == bassCopy[i].noteOffset,
                "Kick generation leaves Bass notes unchanged");
    }

    juce::MemoryBlock bassData, kickData;
    bass.serialize(bassData);
    kick.serialize(kickData);
    const auto bassRestored = vstengine::sequence::Sequence::deserialize(bassData);
    const auto kickRestored = vstengine::sequence::Sequence::deserialize(kickData);
    REQUIRE(bassRestored.size() == bass.size(), "Bass sequence serializes alone");
    REQUIRE(kickRestored.size() == kick.size(), "Kick sequence serializes alone");

    std::cout << "PartSequence tests passed (" << checks << ")\n";
    return EXIT_SUCCESS;
}
