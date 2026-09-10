#include "preset/PresetManager.h"
#include "core/KickParameterIds.h"
#include "core/SoundParameterIds.h"
#include "parts/PartState.h"
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
               "kickMidiChannel", "matchBassTimingOffsetMs" }) {
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
        return juce::jlimit(0.0f, 1.0f, plainValue / 10.0f);
    }

    juce::ValueTree state;
};

float readParam(const juce::ValueTree& state, const char* id)
{
    return static_cast<float>(static_cast<double>(
        state.getChildWithProperty("id", juce::var(id)).getProperty("value")));
}

void setParam(juce::ValueTree& state, const char* id, const float value)
{
    state.getChildWithProperty("id", juce::var(id))
        .setProperty("value", juce::var(value), nullptr);
}

void requireStatesEqual(const juce::ValueTree& expected,
                        const juce::ValueTree& actual)
{
    require(expected.getNumChildren() == actual.getNumChildren(),
            "parameter count preserved");
    for (int i = 0; i < expected.getNumChildren(); ++i) {
        const auto id = expected.getChild(i).getProperty("id").toString();
        require(std::abs(readParam(expected, id.toRawUTF8())
                         - readParam(actual, id.toRawUTF8())) < 1e-6f,
                "parameter value preserved");
    }
}

} // namespace

int main()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("VstEnginePresetTests");
    // 13: Full preset restores every Part, including independent sequences.
    {
        FakeStore store;
        vstengine::parts::PartRegistry parts;
        parts[0].midiChannel = 5;
        parts[0].mute = true;
        parts[0].sequence[1].gate = true;
        parts[1].midiChannel = 9;
        parts[1].solo = true;
        parts[1].level = 0.42f;
        parts[1].sequence[7].gate = true;
        vstengine::PresetManager manager(
            store, parts[0].sequence, dir,
            { [&parts] {
                  return vstengine::parts::PartState::serialize(parts);
              },
              [&parts](const juce::ValueTree& state) {
                  return vstengine::parts::PartState::restore(state, parts);
              } });

        require(manager.saveFullPreset("All Parts"),
                "full preset with Parts saves");
        parts = vstengine::parts::PartRegistry {};
        require(manager.loadFullPreset("All Parts"),
                "full preset with Parts loads");
        require(parts[0].midiChannel == 5 && parts[0].mute
                    && parts[0].sequence[1].gate,
                "full preset restores Bass Part");
        require(parts[1].midiChannel == 9 && parts[1].solo
                    && std::abs(parts[1].level - 0.42f) < 1.0e-6f
                    && parts[1].sequence[7].gate,
                "full preset restores Kick Part");
    }

    dir.deleteRecursively();
    dir.createDirectory();

    // 1: Factory preset list + factory sound preset isolation.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        seq[0].noteOffset = 5;
        vstengine::PresetManager manager(store, seq, dir);

        const auto names = manager.getFactoryPresetNames();
        require(names.size() == 10, "all factory presets listed");
        for (const auto& name : { "Tight Rolling", "Dark Rolling", "Short Punch",
                                  "Deep Rolling", "Hi-Tech Tight", "Psytrance",
                                  "Dark Psy", "Progressive Psy", "Hi-Tech",
                                  "Classic Trance" })
            require(names.contains(name), "expected factory preset listed");
        require(manager.loadFactoryPreset("Tight Rolling"),
                "bass factory preset loads");
        require(std::abs(readParam(store.state, "drive") - 0.22f) < 1e-6f,
                "bass factory applies expected drive");
        require(readParam(store.state, "midiMode") == 0.0f,
                "sound preset leaves midiMode untouched");
        require(seq[0].noteOffset == 5,
                "sound preset leaves sequence untouched");
        require(!manager.loadFactoryPreset("Definitely Missing"),
                "unknown factory preset rejected");
    }

    // 2: Bass Sound preset changes Bass only and creates a real file.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);

        setParam(store.state, "drive", 0.75f);
        setParam(store.state, "kickPitchEnd", 0.35f);
        setParam(store.state, "midiMode", 0.4f);
        require(manager.saveSoundPreset(
                    "Round Trip Bass", vstengine::PresetManager::SoundEngine::bass),
                "bass sound preset saves");
        require(dir.getChildFile("Round Trip Bass.xml").existsAsFile(),
                "bass sound preset creates file");
        const auto bassXml = juce::parseXML(
            dir.getChildFile("Round Trip Bass.xml").loadFileAsString());
        require(bassXml != nullptr
                    && bassXml->getStringAttribute("engine") == "bass",
                "bass sound preset carries engine metadata");

        setParam(store.state, "drive", 0.1f);
        setParam(store.state, "kickPitchEnd", 0.8f);
        setParam(store.state, "midiMode", 0.9f);
        require(manager.loadSoundPreset(
                    vstengine::PresetManager::EngineType::bass,
                    "Round Trip Bass"),
                "sound preset loads");
        require(std::abs(readParam(store.state, "drive") - 0.75f) < 1e-6f,
                "sound preset round-trips drive value");
        require(readParam(store.state, "kickPitchEnd") == 0.8f,
                "bass sound preset leaves Kick untouched");
        require(readParam(store.state, "midiMode") == 0.9f,
                "bass sound preset leaves globals untouched");
        require(manager.getCurrentPresetName() == "Round Trip Bass",
                "current preset name tracked");
    }

    // 3: Kick Sound preset changes Kick only.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        setParam(store.state, "kickPitchEnd", 0.35f);
        setParam(store.state, "drive", 0.75f);
        require(manager.saveSoundPreset(
                    "Round Trip Kick", vstengine::PresetManager::SoundEngine::kick),
                "kick sound preset saves");
        const auto kickXml = juce::parseXML(
            dir.getChildFile("Round Trip Kick.xml").loadFileAsString());
        require(kickXml != nullptr
                    && kickXml->getStringAttribute("engine") == "kick",
                "kick sound preset carries engine metadata");
        setParam(store.state, "kickPitchEnd", 0.1f);
        setParam(store.state, "drive", 0.2f);
        require(manager.loadSoundPreset(
                    vstengine::PresetManager::EngineType::kick,
                    "Round Trip Kick"),
                "kick sound preset loads");
        require(std::abs(readParam(store.state, "kickPitchEnd") - 0.35f)
                    < 1e-6f,
                "kick sound value restored");
        require(readParam(store.state, "drive") == 0.2f,
                "kick sound preset leaves Bass untouched");
    }

    // 4: Full preset carries complete APVTS state + canonical sequence.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        seq[3].gate = true;
        seq[3].noteOffset = 12;
        seq[3].ratchetCount = 4;
        seq[3].slideDuration = 1.5f;
        seq.setSelectedRange(2, 9);
        seq.setTimingMode(vstengine::sequence::TimingMode::triplet);
        vstengine::PresetManager manager(store, seq, dir);

        for (int i = 0; i < store.state.getNumChildren(); ++i)
            store.state.getChild(i).setProperty(
                "value", juce::var(0.01f * static_cast<float>(i + 1)), nullptr);
        const auto expectedState = store.state.createCopy();

        require(manager.saveFullPreset("Round Trip Full"),
                "full preset saves");

        // Clobber live sequence + state, then restore.
        seq.clear();
        seq.clearSelection();
        seq.setTimingMode(vstengine::sequence::TimingMode::sixteenth);
        for (int i = 0; i < store.state.getNumChildren(); ++i)
            store.state.getChild(i).setProperty("value", juce::var(0.99f), nullptr);
        require(manager.loadFullPreset("Round Trip Full"),
                "full preset loads");
        requireStatesEqual(expectedState, store.state);
        require(seq.size() == 16, "full preset restores size");
        require(seq[3].gate && seq[3].noteOffset == 12
                    && seq[3].ratchetCount == 4,
                "full preset restores step data");
        require(std::abs(seq[3].slideDuration - 1.5f) < 1e-6f,
                "full preset restores slide");
        require(seq.hasSelection() && seq.getSelectedStart() == 2
                    && seq.getSelectedEnd() == 9,
                "full preset restores selection");
        require(seq.getTimingMode() == vstengine::sequence::TimingMode::triplet,
                "full preset restores timing mode");
    }

    // 5: Corrupt full preset fails safely (all live state untouched).
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        setParam(store.state, "drive", 0.7f);
        require(manager.saveFullPreset("Corrupt Me"), "corrupt fixture saves");

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
        require(readParam(store.state, "drive") == 0.7f,
                "live parameter state untouched on rejection");
    }

    // 6: Kick factory presets set Kick params only.
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
        require(std::abs(readParam(store.state, "kickPitchStart") - 1.0f)
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
        require(std::abs(readParam(store.state, "kickPitchStart") - 1.0f)
                    < 1e-5f,
                "bass factory leaves kick params untouched");
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

    // 8: Typed catalog reports source, kind and sound engine from real data.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        require(manager.saveSoundPreset(
                    "Catalog Bass", vstengine::PresetManager::SoundEngine::bass),
                "catalog bass saves");
        require(manager.saveSoundPreset(
                    "Catalog Kick", vstengine::PresetManager::SoundEngine::kick),
                "catalog kick saves");
        require(manager.saveFullPreset("Catalog Full"), "catalog full saves");

        bool foundFactoryBass = false;
        bool foundUserBass = false;
        bool foundUserKick = false;
        bool foundUserFull = false;
        for (const auto& entry : manager.getPresets()) {
            if (entry.name == "Tight Rolling")
                foundFactoryBass = entry.isReadOnly()
                    && entry.engine == vstengine::PresetManager::SoundEngine::bass;
            if (entry.name == "Catalog Bass")
                foundUserBass = !entry.isReadOnly()
                    && entry.engine == vstengine::PresetManager::SoundEngine::bass;
            if (entry.name == "Catalog Kick")
                foundUserKick = !entry.isReadOnly()
                    && entry.engine == vstengine::PresetManager::SoundEngine::kick;
            if (entry.name == "Catalog Full")
                foundUserFull = !entry.isReadOnly()
                    && entry.kind == vstengine::PresetManager::PresetKind::full;
        }
        require(foundFactoryBass, "catalog identifies factory Bass");
        require(foundUserBass, "catalog identifies user Bass");
        require(foundUserKick, "catalog identifies user Kick");
        require(foundUserFull, "catalog identifies user Full");
    }

    // 9: Rename updates disk and embedded name; Delete removes real file.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        setParam(store.state, "drive", 0.73f);
        require(manager.saveSoundPreset(
                    "Before Rename", vstengine::PresetManager::SoundEngine::bass),
                "rename fixture saves");
        require(manager.renamePreset("Before Rename", "After Rename"),
                "user preset renames");
        require(!dir.getChildFile("Before Rename.xml").existsAsFile(),
                "old preset path absent");
        require(dir.getChildFile("After Rename.xml").existsAsFile(),
                "new preset path exists");
        require(!manager.getSoundPresetNames().contains("Before Rename"),
                "old preset name absent from refresh");
        require(manager.getSoundPresetNames().contains("After Rename"),
                "new preset name appears after refresh");
        setParam(store.state, "drive", 0.1f);
        require(manager.loadSoundPreset("After Rename"),
                "renamed preset loads");
        require(std::abs(readParam(store.state, "drive") - 0.73f) < 1e-6f,
                "renamed preset retains data");
        require(manager.deletePreset("After Rename"), "user preset deletes");
        require(!dir.getChildFile("After Rename.xml").existsAsFile(),
                "deleted preset file absent");
        require(!manager.getSoundPresetNames().contains("After Rename"),
                "deleted preset absent after refresh");
    }

    // 10: Factory names are protected and invalid names report exact errors.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        const auto rename = manager.renamePreset("Tight Rolling", "Changed");
        require(rename.error == vstengine::PresetManager::Error::readOnly,
                "factory rename rejected");
        const auto remove = manager.deletePreset("Psytrance");
        require(remove.error == vstengine::PresetManager::Error::readOnly,
                "factory delete rejected");
        const auto overwrite = manager.saveFullPreset("Tight Rolling");
        require(overwrite.error == vstengine::PresetManager::Error::readOnly,
                "factory overwrite rejected");
        const auto invalid = manager.saveFullPreset("Bad/Name");
        require(invalid.error == vstengine::PresetManager::Error::invalidName,
                "invalid name rejected");
    }

    // 11: Explicit engine target filters catalogs and rejects cross-loads.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);

        const auto bassFactory = manager.getFactoryPresetNames(
            vstengine::PresetManager::EngineType::bass);
        const auto kickFactory = manager.getFactoryPresetNames(
            vstengine::PresetManager::EngineType::kick);
        require(bassFactory.size() == 5 && bassFactory.contains("Tight Rolling")
                    && !bassFactory.contains("Psytrance"),
                "Bass factory catalog excludes Kick presets");
        require(kickFactory.size() == 5 && kickFactory.contains("Psytrance")
                    && !kickFactory.contains("Tight Rolling"),
                "Kick factory catalog excludes Bass presets");

        setParam(store.state, "drive", 0.64f);
        setParam(store.state, "kickPitchEnd", 0.27f);
        require(manager.saveSoundPreset(
                    vstengine::PresetManager::EngineType::kick,
                    "Explicit Kick"),
                "explicit Kick preset saves");
        require(manager.getSoundPresetNames(
                    vstengine::PresetManager::EngineType::kick)
                    .contains("Explicit Kick"),
                "Kick user catalog contains Kick preset");
        require(!manager.getSoundPresetNames(
                    vstengine::PresetManager::EngineType::bass)
                    .contains("Explicit Kick"),
                "Bass user catalog excludes Kick preset");

        setParam(store.state, "drive", 0.11f);
        setParam(store.state, "kickPitchEnd", 0.91f);
        const auto wrongSound = manager.loadSoundPreset(
            vstengine::PresetManager::EngineType::bass, "Explicit Kick");
        require(wrongSound.error
                    == vstengine::PresetManager::Error::wrongEngine,
                "Kick user preset rejected by Bass target");
        require(readParam(store.state, "drive") == 0.11f
                    && readParam(store.state, "kickPitchEnd") == 0.91f,
                "wrong-engine user load leaves both engines untouched");

        const auto wrongFactory = manager.loadFactoryPreset(
            vstengine::PresetManager::EngineType::bass, "Psytrance");
        require(wrongFactory.error
                    == vstengine::PresetManager::Error::fileNotFound,
                "Kick factory preset rejected by Bass target");

        setParam(store.state, "drive", 0.23f);
        require(manager.saveSoundPreset(
                    vstengine::PresetManager::EngineType::bass, "Shared Name"),
                "Bass can save shared display name");
        setParam(store.state, "kickPitchEnd", 0.76f);
        require(manager.saveSoundPreset(
                    vstengine::PresetManager::EngineType::kick, "Shared Name"),
                "Kick can save shared display name");
        int sharedEntries = 0;
        for (const auto& entry : manager.getPresets())
            if (entry.source == vstengine::PresetManager::PresetSource::user
                && entry.name == "Shared Name")
                ++sharedEntries;
        require(sharedEntries == 2,
                "Bass and Kick banks keep same-name presets independently");
    }

    // 12: Malformed and future presets fail with useful errors, no state change.
    {
        FakeStore store;
        vstengine::sequence::Sequence seq(16);
        vstengine::PresetManager manager(store, seq, dir);
        setParam(store.state, "drive", 0.61f);
        dir.getChildFile("Malformed.xml").replaceWithText("not xml");
        const auto malformed = manager.loadSoundPreset("Malformed");
        require(malformed.error == vstengine::PresetManager::Error::invalidPreset,
                "malformed preset reports invalid preset");
        require(readParam(store.state, "drive") == 0.61f,
                "malformed preset leaves state untouched");

        juce::XmlElement future("VstEnginePreset");
        future.setAttribute("version", 99);
        future.setAttribute("type", "sound");
        future.createNewChildElement("name")->setText("Future");
        dir.getChildFile("Future.xml").replaceWithText(future.toString());
        const auto unsupported = manager.loadSoundPreset("Future");
        require(unsupported.error
                    == vstengine::PresetManager::Error::unsupportedVersion,
                "future preset reports unsupported version");
        require(readParam(store.state, "drive") == 0.61f,
                "future preset leaves state untouched");

        require(manager.saveFullPreset("Empty Parameters"),
                "invalid parameter fixture saves");
        const auto emptyFile = dir.getChildFile("Empty Parameters.xml");
        auto emptyXml = juce::parseXML(emptyFile.loadFileAsString());
        require(emptyXml != nullptr, "invalid parameter fixture parses");
        auto* tree = emptyXml->getChildByName("parameters")
                         ->getChildByName("PARAMETERS");
        while (tree->getNumChildElements() > 0)
            tree->removeChildElement(tree->getChildElement(0), true);
        emptyFile.replaceWithText(emptyXml->toString());
        const auto empty = manager.loadFullPreset("Empty Parameters");
        require(empty.error == vstengine::PresetManager::Error::invalidPreset,
                "empty parameter tree rejected");
        require(readParam(store.state, "drive") == 0.61f,
                "empty parameter tree leaves state untouched");

        require(manager.saveFullPreset("Bad Numeric"),
                "bad numeric fixture saves");
        const auto badNumericFile = dir.getChildFile("Bad Numeric.xml");
        auto badNumericXml = juce::parseXML(badNumericFile.loadFileAsString());
        auto* badTree = badNumericXml->getChildByName("parameters")
                            ->getChildByName("PARAMETERS");
        badTree->getChildElement(0)->setAttribute("value", "1e");
        badNumericFile.replaceWithText(badNumericXml->toString());
        require(manager.loadFullPreset("Bad Numeric").error
                    == vstengine::PresetManager::Error::invalidPreset,
                "partially parsed numeric value rejected");
    }

    dir.deleteRecursively();
    std::cout << "Preset tests passed (" << testsPassed << "/" << testsRun
              << ")\n";
    return EXIT_SUCCESS;
}
