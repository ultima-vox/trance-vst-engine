#include "preset/PresetManager.h"
#include "core/KickParameterIds.h"
#include "core/SoundParameterIds.h"
#include "parts/PartState.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int testsRun = 0;
int testsPassed = 0;
#define REQUIRE(condition, message)                                           \
    do {                                                                       \
        ++testsRun;                                                            \
        if (!(condition)) {                                                    \
            std::cerr << "FAILED: " << message << " line " << __LINE__ << '\n'; \
            std::exit(EXIT_FAILURE);                                           \
        }                                                                      \
        ++testsPassed;                                                         \
    } while (false)

class FakeStore final : public vstengine::PresetStateStore {
public:
    FakeStore()
    {
        state = juce::ValueTree("PARAMETERS");
        auto add = [this](const char* id, float value) {
            juce::ValueTree parameter("PARAM");
            parameter.setProperty("id", id, nullptr);
            parameter.setProperty("value", value, nullptr);
            state.appendChild(parameter, nullptr);
        };
        for (const auto* id : vstengine::core::soundParameterIds) add(id, 0.5f);
        for (const auto* id : vstengine::core::kickSoundParameterIds) add(id, 0.5f);
        for (const auto* id : { "midiMode", "midiChannel", "rootNote",
                                "rngSeed", "kickMidiChannel",
                                "matchBassTimingOffsetMs" })
            add(id, 0.0f);
    }
    juce::ValueTree copyState() const override { return state; }
    void replaceState(juce::ValueTree replacement) override
    {
        state = std::move(replacement);
    }
    float convertTo0to1(const char*, float value) const override
    {
        return juce::jlimit(0.0f, 1.0f, value / 10.0f);
    }
    juce::ValueTree state;
};

float read(const juce::ValueTree& state, const char* id)
{
    return static_cast<float>(static_cast<double>(
        state.getChildWithProperty("id", id).getProperty("value")));
}
void write(juce::ValueTree& state, const char* id, float value)
{
    state.getChildWithProperty("id", id).setProperty("value", value, nullptr);
}
} // namespace

int main()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("VoxTranceEngine7APresetTests");
    dir.deleteRecursively();
    REQUIRE(dir.createDirectory(), "test directory created");

    FakeStore store;
    vstengine::sequence::Sequence sequence(16);
    vstengine::parts::PartRegistry parts;
    vstengine::PresetManager manager(
        store, sequence, dir,
        { [&parts] { return vstengine::parts::PartState::serialize(parts); },
          [&parts](const juce::ValueTree& state) {
              return vstengine::parts::PartState::restore(state, parts);
          } });

    const auto factories = manager.getFactoryPresetNames();
    REQUIRE(factories.size() == 5, "only Bass factory presets remain active");
    for (const auto& name : { "Tight Rolling", "Dark Rolling", "Short Punch",
                              "Deep Rolling", "Hi-Tech Tight" })
        REQUIRE(factories.contains(name), "Bass factory preset listed");
    REQUIRE(manager.getFactoryPresetNames(
                vstengine::PresetManager::EngineType::kick).isEmpty(),
            "Kick factory bank absent");
    REQUIRE(!manager.loadFactoryPreset("Psytrance"),
            "removed Kick factory cannot load");

    write(store.state, "drive", 0.75f);
    write(store.state, "kickPitchEnd", 0.35f);
    REQUIRE(manager.saveSoundPreset("Round Trip Bass"), "Bass sound saves");
    write(store.state, "drive", 0.1f);
    write(store.state, "kickPitchEnd", 0.8f);
    REQUIRE(manager.loadSoundPreset("Round Trip Bass"), "Bass sound loads");
    REQUIRE(std::abs(read(store.state, "drive") - 0.75f) < 1.0e-6f,
            "Bass value restored");
    REQUIRE(read(store.state, "kickPitchEnd") == 0.8f,
            "Bass preset cannot leak into dormant Kick state");

    const auto saveKick = manager.saveSoundPreset(
        "New Kick", vstengine::PresetManager::EngineType::kick);
    REQUIRE(saveKick.error == vstengine::PresetManager::Error::wrongEngine,
            "new Kick content rejected");

    juce::XmlElement legacyKick("VstEnginePreset");
    legacyKick.setAttribute("version", 2);
    legacyKick.setAttribute("type", "sound");
    legacyKick.setAttribute("engine", "kick");
    legacyKick.createNewChildElement("name")->setText("Legacy Kick");
    auto* parameters = legacyKick.createNewChildElement("parameters")
                           ->createNewChildElement("PARAMETERS");
    auto* parameter = parameters->createNewChildElement("PARAM");
    parameter->setAttribute("id", "kickPitchEnd");
    parameter->setAttribute("value", "0.25");
    REQUIRE(dir.getChildFile("Legacy Kick.xml").replaceWithText(
                legacyKick.toString()),
            "legacy Kick fixture written");
    bool legacyKickVisible = false;
    for (const auto& entry : manager.getPresets())
        legacyKickVisible = legacyKickVisible || entry.name == "Legacy Kick";
    REQUIRE(!legacyKickVisible, "legacy Kick preset hidden from active catalog");
    REQUIRE(manager.loadSoundPreset(vstengine::PresetManager::EngineType::kick,
                                    "Legacy Kick"),
            "legacy Kick preset remains explicitly readable for migration");

    parts[0].midiChannel = 5;
    parts[0].sequence[3].gate = true;
    parts[1].midiChannel = 9;
    parts[1].sequence[6].gate = true;
    write(store.state, "matchBassTimingOffsetMs", 0.42f);
    REQUIRE(manager.saveFullPreset("Legacy Project"), "full state saves");
    parts = vstengine::parts::PartRegistry {};
    write(store.state, "matchBassTimingOffsetMs", 0.0f);
    REQUIRE(manager.loadFullPreset("Legacy Project"), "full state loads");
    REQUIRE(parts[0].midiChannel == 5 && parts[0].sequence[3].gate,
            "Bass part round-trips");
    REQUIRE(parts[1].midiChannel == 9 && parts[1].sequence[6].gate,
            "removed Kick payload retained for migration");
    REQUIRE(std::abs(read(store.state, "matchBassTimingOffsetMs") - 0.42f)
                < 1.0e-6f,
            "dormant MATCH value round-trips");

    const auto fullFile = dir.getChildFile("Legacy Project.xml");
    const auto before = store.state.createCopy();
    auto xml = juce::parseXML(fullFile.loadFileAsString());
    REQUIRE(xml != nullptr, "full fixture parses");
    auto* sequenceElement = xml->getChildByName("sequence");
    sequenceElement->deleteAllTextElements();
    sequenceElement->addTextElement("not-base64");
    REQUIRE(fullFile.replaceWithText(xml->toString()), "corrupt fixture written");
    const auto corrupt = manager.loadFullPreset("Legacy Project");
    REQUIRE(corrupt.error == vstengine::PresetManager::Error::invalidPreset,
            "corrupt full preset rejected transactionally");
    REQUIRE(std::abs(read(store.state, "matchBassTimingOffsetMs")
                     - read(before, "matchBassTimingOffsetMs")) < 1.0e-6f,
            "failed load leaves state unchanged");

    dir.deleteRecursively();
    std::cout << "Preset tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}
