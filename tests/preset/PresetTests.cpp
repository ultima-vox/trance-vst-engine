#include "preset/PresetManager.h"
#include "core/KickParameterIds.h"
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
        // Kick engine sound parameters (issue #11 PHASE 5).
        for (int i = 0; i < vstengine::core::numKickSoundParameterIds; ++i) {
            auto p = juce::ValueTree("PARAM");
            p.setProperty(
                "id", juce::var(vstengine::core::kickSoundParameterIds[i]),
                nullptr);
            p.setProperty("value", juce::var(0.5f), nullptr);
            state.appendChild(p, nullptr);
        }
        // Global (non-sound) settings present in full state.
        for (const char* id :
             { "midiMode", "midiChannel", "rootNote", "rngSeed",
               "kickMidiChannel" }) {
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

    // 5: Kick factory presets (issue #11 PHASE 5) set kick params only.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);

        const auto names = manager.getFactoryPresetNames();
        require(names.contains("Psytrance"), "kick factory preset listed");
        require(names.contains("Dark Psy"), "dark psy kick preset listed");
        require(names.contains("Progressive Psy"),
                "progressive kick preset listed");
        require(names.contains("Hi-Tech"), "hi-tech kick preset listed");
        require(names.contains("Classic Trance"),
                "classic trance kick preset listed");

        require(manager.loadFactoryPreset("Psytrance"),
                "kick factory preset loads");
        // convertTo0to1 divides by 10, so 18 st -> 1.8 normalized.
        require(std::abs(readParam(store.state, "kickPitchStart") - 1.8f)
                    < 1e-5f,
                "kick factory sets kickPitchStart");
        require(readParam(store.state, "drive") == 0.5f,
                "kick factory leaves bass sound params untouched");
        require(readParam(store.state, "midiMode") == 0.0f,
                "kick factory leaves globals untouched");

        // Bass factory presets never touch kick params: the kick value set
        // by the previous kick preset load must survive unchanged.
        require(manager.loadFactoryPreset("Tight Rolling"),
                "bass factory preset still loads");
        require(std::abs(readParam(store.state, "kickPitchStart") - 1.8f)
                    < 1e-5f,
                "bass factory leaves kick params untouched");
    }

    // 6: Sound preset round-trip includes kick parameters (v1 extension).
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);

        store.state.getChildWithProperty("id", juce::var("kickPitchEnd"))
            .setProperty("value", juce::var(0.35f), nullptr);
        store.state.getChildWithProperty("id", juce::var("drive"))
            .setProperty("value", juce::var(0.75f), nullptr);
        manager.saveSoundPreset("Kick Round Trip");

        store.state.getChildWithProperty("id", juce::var("kickPitchEnd"))
            .setProperty("value", juce::var(0.1f), nullptr);
        store.state.getChildWithProperty("id", juce::var("drive"))
            .setProperty("value", juce::var(0.2f), nullptr);

        require(manager.loadSoundPreset("Kick Round Trip"),
                "sound preset with kick params loads");
        require(std::abs(readParam(store.state, "kickPitchEnd") - 0.35f)
                    < 1e-6f,
                "kick param round-trips through sound preset");
        require(std::abs(readParam(store.state, "drive") - 0.75f) < 1e-6f,
                "bass param round-trips through sound preset");
    }

    // 7: Sound preset XML stays loadable without kick entries (v1 backward
    //    compatibility: missing kick params keep live values).
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);

        juce::XmlElement xml("VstEnginePreset");
        xml.setAttribute("version", 1);
        xml.setAttribute("type", "sound");
        xml.createNewChildElement("name")->setText("Legacy Sound");
        auto* paramsEl = xml.createNewChildElement("parameters");
        auto* paramsTree = paramsEl->createNewChildElement("PARAMETERS");
        auto* paramEl = paramsTree->createNewChildElement("PARAM");
        paramEl->setAttribute("id", "drive");
        paramEl->setAttribute("value", juce::String(0.9f, 8));

        // Write the pre-kick preset to disk and load it through the regular
        // file-based loader (same path Cubase restore would take).
        const auto file = dir.getChildFile("Legacy Sound.xml");
        file.replaceWithText(xml.toString());
        require(manager.loadSoundPreset("Legacy Sound"),
                "pre-kick v1 sound preset loads");
        require(std::abs(readParam(store.state, "drive") - 0.9f) < 1e-5f,
                "legacy preset applies bass params");
        require(readParam(store.state, "kickPitchStart") == 0.5f,
                "legacy preset keeps live kick values");
    }

    dir.deleteRecursively();
    std::cout << "Preset tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}
