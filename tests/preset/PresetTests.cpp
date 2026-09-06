#include "preset/PresetManager.h"
#include "core/SoundParameterIds.h"
#include <juce_core/juce_core.h>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int testsRun = 0;
int testsPassed = 0;

#define require(cond, msg)                                                   \
    do {                                                                     \
        ++testsRun;                                                          \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL: " << msg << " at line " << __LINE__ << "\n"; \
            std::exit(EXIT_FAILURE);                                         \
        }                                                                    \
        ++testsPassed;                                                       \
    } while (0)

// Minimal APVTS-shaped parameter store: ValueTree("PARAMETERS") whose children
// are ValueTree("PARAM") with "id" and "value" properties (normalized floats).
class FakeStore final : public vstengine::PresetStateStore {
public:
    FakeStore()
    {
        state = juce::ValueTree("PARAMETERS");
        for (int i = 0; i < vstengine::core::numSoundParameterIds; ++i) {
            auto p = juce::ValueTree("PARAM");
            p.setProperty("id",
                          juce::var(vstengine::core::soundParameterIds[i]),
                          nullptr);
            p.setProperty("value", juce::var(0.5f), nullptr);
            state.appendChild(p, nullptr);
        }
        // Global (non-sound) settings present in full state.
        for (const char* id :
             { "midiMode", "midiChannel", "rootNote", "rngSeed" }) {
            auto p = juce::ValueTree("PARAM");
            p.setProperty("id", juce::var(id), nullptr);
            p.setProperty("value", juce::var(0.0f), nullptr);
            state.appendChild(p, nullptr);
        }
    }

    [[nodiscard]] juce::ValueTree copyState() const override { return state; }
    void replaceState(juce::ValueTree newState) override { state = newState; }
    [[nodiscard]] float convertTo0to1(const char* parameterId,
                                      float plainValue) const override
    {
        juce::ignoreUnused(parameterId);
        return plainValue / 10.0f;
    }

    juce::ValueTree state;
};

float readParam(const juce::ValueTree& state, const char* id)
{
    return static_cast<float>(static_cast<double>(
        state.getChildWithProperty("id", juce::var(id)).getProperty("value")));
}

} // namespace

int main()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("VstEnginePresetTests");
    dir.deleteRecursively();
    dir.createDirectory();

    // 1: Factory preset list + factory sound preset isolation.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        seq[0].noteOffset = 5;
        vstengine::PresetManager manager(store, seq, dir);

        const auto names = manager.getFactoryPresetNames();
        require(names.size() > 0, "factory preset list not empty");
        require(manager.loadFactoryPreset(names[0]), "factory preset loads");
        require(readParam(store.state, "midiMode") == 0.0f,
                "sound preset leaves midiMode untouched");
        require(seq[0].noteOffset == 5,
                "sound preset leaves sequence untouched");
        require(!manager.loadFactoryPreset("Definitely Missing"),
                "unknown factory preset rejected");
    }

    // 2: Sound preset save/load round-trip (v1 XML schema).
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);

        store.state.getChildWithProperty("id", juce::var("drive"))
            .setProperty("value", juce::var(0.75f), nullptr);
        manager.saveSoundPreset("Round Trip Sound");

        store.state.getChildWithProperty("id", juce::var("drive"))
            .setProperty("value", juce::var(0.1f), nullptr);
        require(manager.loadSoundPreset("Round Trip Sound"),
                "sound preset loads");
        require(std::abs(readParam(store.state, "drive") - 0.75f) < 1e-6f,
                "sound preset round-trips drive value");
        require(manager.getCurrentPresetName() == "Round Trip Sound",
                "current preset name tracked");
    }

    // 3: Full preset carries the canonical sequence (round-trip equality).
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        seq[3].gate = true;
        seq[3].noteOffset = 12;
        seq[3].ratchetCount = 4;
        seq[3].slideDuration = 1.5f;
        seq.setSelectedRange(2, 9);
        vstengine::PresetManager manager(store, seq, dir);

        manager.saveFullPreset("Round Trip Full");

        // Clobber live sequence + state, then restore.
        seq.clear();
        seq.clearSelection();
        require(manager.loadFullPreset("Round Trip Full"),
                "full preset loads");
        require(seq.size() == 16, "full preset restores size");
        require(seq[3].gate && seq[3].noteOffset == 12
                    && seq[3].ratchetCount == 4,
                "full preset restores step data");
        require(std::abs(seq[3].slideDuration - 1.5f) < 1e-6f,
                "full preset restores slide");
        require(seq.hasSelection() && seq.getSelectedStart() == 2
                    && seq.getSelectedEnd() == 9,
                "full preset restores selection");
    }

    // 4: Corrupt full preset fails safely (live state untouched).
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        manager.saveFullPreset("Corrupt Me");

        const auto file = dir.getChildFile("Corrupt Me.xml");
        require(file.existsAsFile(), "preset file written");
        auto xml = juce::parseXML(file.loadFileAsString());
        require(xml != nullptr, "preset parses");
        auto* seqEl = xml->getChildByName("sequence");
        while (seqEl->getNumChildElements() > 0)
            seqEl->removeChildElement(seqEl->getChildElement(0), true);
        seqEl->addTextElement("bm90LWEtc2VxdWVuY2U=");
        file.replaceWithText(xml->toString());

        require(!manager.loadFullPreset("Corrupt Me"),
                "corrupt sequence rejected");
        require(seq.size() == 16, "live sequence untouched on rejection");
    }

    dir.deleteRecursively();
    std::cout << "Preset tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}
