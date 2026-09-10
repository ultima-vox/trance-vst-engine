#include "parts/PartState.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int checks {};
#define REQUIRE(condition, message) do { ++checks; if (!(condition)) { std::cerr << "FAIL: " << message << " line " << __LINE__ << '\n'; return EXIT_FAILURE; } } while (false)
}

int main()
{
    using namespace vstengine::parts;
    PartRegistry original;
    auto* bass = original.find(PartId::bass);
    auto* kick = original.find(PartId::kick);
    bass->midiChannel = 5;
    bass->mute = true;
    bass->level = 0.75f;
    bass->pan = -0.25f;
    bass->sequence[0].gate = true;
    kick->midiChannel = 9;
    kick->solo = true;
    kick->locked = true;
    kick->sequence[3].velocity = 0.42f;

    const auto state = PartState::serialize(original);
    REQUIRE(static_cast<int>(state.getProperty("version"))
                == PartState::currentVersion,
            "state has current version");

    PartRegistry restored;
    REQUIRE(PartState::restore(state, restored), "current state restores");
    REQUIRE(restored.find(PartId::bass)->midiChannel == 5,
            "Bass channel restores");
    REQUIRE(restored.find(PartId::bass)->mute, "Bass mute restores");
    REQUIRE(std::abs(restored.find(PartId::bass)->level - 0.75f) < 1.0e-6f,
            "Bass level restores");
    REQUIRE(restored.find(PartId::bass)->sequence[0].gate,
            "Bass sequence restores");
    REQUIRE(restored.find(PartId::kick)->midiChannel == 9,
            "Kick channel restores");
    REQUIRE(restored.find(PartId::kick)->solo
                && restored.find(PartId::kick)->locked,
            "Kick flags restore");
    REQUIRE(std::abs(restored.find(PartId::kick)->sequence[3].velocity - 0.42f)
                < 1.0e-6f,
            "Kick sequence restores");

    auto v1 = state.createCopy();
    v1.setProperty("version", 1, nullptr);
    for (int i = 0; i < v1.getNumChildren(); ++i)
        v1.getChild(i).removeProperty("enabled", nullptr);
    restored.find(PartId::bass)->enabled = false;
    REQUIRE(PartState::restore(v1, restored), "v1 state migrates");
    REQUIRE(restored.find(PartId::bass)->enabled,
            "v1 migration defaults enabled true");

    const int beforeChannel = restored.find(PartId::bass)->midiChannel;
    auto corrupt = state.createCopy();
    corrupt.getChild(0).setProperty("midiChannel", 17, nullptr);
    REQUIRE(!PartState::restore(corrupt, restored), "invalid channel rejected");
    REQUIRE(restored.find(PartId::bass)->midiChannel == beforeChannel,
            "failed restore is transactional");

    auto future = state.createCopy();
    future.setProperty("version", PartState::currentVersion + 1, nullptr);
    REQUIRE(!PartState::restore(future, restored), "future version rejected");

    auto duplicate = state.createCopy();
    duplicate.getChild(1).setProperty("id", static_cast<int>(PartId::bass), nullptr);
    duplicate.getChild(1).setProperty("engineType",
                                      static_cast<int>(EngineType::bass), nullptr);
    REQUIRE(!PartState::restore(duplicate, restored), "duplicate Part rejected");

    std::cout << "PartState tests passed (" << checks << ")\n";
    return EXIT_SUCCESS;
}
