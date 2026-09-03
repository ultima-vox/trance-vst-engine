#include "PresetManager.h"
#include "../PluginProcessor.h"
#include <array>
#include <iterator>

namespace vstengine {

namespace {
juce::String encodeSequence(const vstengine::generator::Sequence& seq) {
    juce::MemoryBlock mb;
    seq.serialize(mb);
    return mb.toBase64Encoding();
}

// Fail-safe decode: only replaces the target sequence when the blob decodes
// AND passes structural validation. Corrupt data never touches live state.
bool decodeSequence(const juce::String& encoded, vstengine::generator::Sequence& seq) {
    juce::MemoryBlock mb;
    if (!mb.fromBase64Encoding(encoded))
        return false;
    if (!vstengine::generator::Sequence::isValidSerialization(mb))
        return false;
    seq = vstengine::generator::Sequence::deserialize(mb);
    return true;
}
}

// --- Preset schema versioning / migration -----------------------------------
//
// The preset XML carries an explicit "version" attribute. Loading dispatches
// on that version: presets at the current version are loaded directly, older
// supported versions are rewritten by a migration chain, and anything newer
// than the current version is rejected cleanly instead of being misread.
//
// When a v2 schema is introduced:
//   1. bump currentPresetVersion to 2,
//   2. write migrateV1ToV2() below and route version-1 presets through it,
//   3. keep the v1 loader reachable through the chain so old presets survive.
// Until then the chain contains only the v1 (current) loader, which is the
// honest state of the format: v1 presets load, there is nothing to migrate yet.
static bool migrateToCurrentVersion(const juce::XmlElement& source,
                                    juce::XmlElement& result)
{
    const auto version = source.getIntAttribute("version", 0);
    switch (version) {
        case 1: // v1 == current schema: load as-is
            result = source;
            return true;
        default:
            // Future/unknown version: reject cleanly (fail safely).
            return false;
    }
}

// Factory psy-bass presets are Sound presets: psy-bass engine sound parameters
// only - no sequence, no global MIDI/generator state. Values are stored as
// plain (denormalized) parameter values here and written into the v1 sound
// preset XML with the parameter's own convertTo0to1() at load time, matching
// exactly how APVTS serializes user presets (which store normalized values).
struct FactorySoundParam {
    const char* id;
    float value;
};

struct FactoryPreset {
    const char* name;
    std::array<FactorySoundParam, 13> params;
};

// Parameter order must match VstEngineAudioProcessor::soundParameterIds.
static const FactoryPreset factoryPresets[] = {
    { "Tight Rolling", { {
        { "drive", 2.2f }, { "release", 0.05f }, { "ampAttack", 0.0005f },
        { "ampDecay", 0.04f }, { "ampSustain", 0.65f },
        { "filterCutoff", 0.55f }, { "filterResonance", 0.5f },
        { "filterDrive", 1.2f }, { "keyTracking", 0.25f },
        { "pitchEnvAmount", 8.0f }, { "pitchEnvTime", 0.02f },
        { "pitchEnvCurve", 2.5f }, { "outputLevel", 1.0f } } } },
    { "Dark Rolling", { {
        { "drive", 3.0f }, { "release", 0.09f }, { "ampAttack", 0.001f },
        { "ampDecay", 0.06f }, { "ampSustain", 0.7f },
        { "filterCutoff", 0.35f }, { "filterResonance", 0.6f },
        { "filterDrive", 1.5f }, { "keyTracking", 0.2f },
        { "pitchEnvAmount", 10.0f }, { "pitchEnvTime", 0.025f },
        { "pitchEnvCurve", 2.0f }, { "outputLevel", 1.0f } } } },
    { "Short Punch", { {
        { "drive", 2.6f }, { "release", 0.02f }, { "ampAttack", 0.0002f },
        { "ampDecay", 0.08f }, { "ampSustain", 0.3f },
        { "filterCutoff", 0.6f }, { "filterResonance", 0.35f },
        { "filterDrive", 0.8f }, { "keyTracking", 0.1f },
        { "pitchEnvAmount", 14.0f }, { "pitchEnvTime", 0.012f },
        { "pitchEnvCurve", 3.0f }, { "outputLevel", 1.0f } } } },
    { "Deep Rolling", { {
        { "drive", 2.0f }, { "release", 0.1f }, { "ampAttack", 0.002f },
        { "ampDecay", 0.14f }, { "ampSustain", 0.55f },
        { "filterCutoff", 0.3f }, { "filterResonance", 0.45f },
        { "filterDrive", 0.7f }, { "keyTracking", 0.3f },
        { "pitchEnvAmount", 6.0f }, { "pitchEnvTime", 0.03f },
        { "pitchEnvCurve", 1.8f }, { "outputLevel", 1.0f } } } },
    { "Hi-Tech Tight", { {
        { "drive", 4.0f }, { "release", 0.025f }, { "ampAttack", 0.0003f },
        { "ampDecay", 0.03f }, { "ampSustain", 0.6f },
        { "filterCutoff", 0.45f }, { "filterResonance", 0.7f },
        { "filterDrive", 2.0f }, { "keyTracking", 0.4f },
        { "pitchEnvAmount", 12.0f }, { "pitchEnvTime", 0.015f },
        { "pitchEnvCurve", 2.2f }, { "outputLevel", 0.9f } } } },
};

PresetManager::PresetManager(VstEngineAudioProcessor& proc,
                             vstengine::generator::Sequence& seq)
    : processor(proc), sequence(seq) {
    presetDirectory = getPresetDirectory();
    if (!presetDirectory.exists())
        presetDirectory.createDirectory();
}

juce::File PresetManager::getPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("UltimaVox").getChildFile("VST-Engine")
        .getChildFile("Presets").getChildFile("User");
}

juce::String PresetManager::makeSafeFilename(const juce::String& name) const {
    juce::String safe;
    for (const auto ch : name) {
        if (juce::CharacterFunctions::isLetter(ch) || juce::CharacterFunctions::isDigit(ch) || ch == ' ' || ch == '-' || ch == '_')
            safe += ch;
    }
    return safe.isEmpty() ? "Unnamed" : safe;
}

void PresetManager::serializeToXml(juce::XmlElement& xml, const PresetKind kind)
{
    xml.setAttribute("version", currentPresetVersion);
    xml.setAttribute("type", kind == PresetKind::sound ? "sound" : "full");
    xml.createNewChildElement("name")->setText(currentPresetName);

    // Sound preset: psy-bass engine sound parameters ONLY (no sequence, no
    // global MIDI/generator state). Full preset: complete APVTS state (which
    // includes the global MIDI/generator settings) plus the canonical
    // sequence - all Phase 0-4 state required by #11.
    auto* paramsEl = xml.createNewChildElement("parameters");
    auto state = processor.parameters().copyState();

    if (kind == PresetKind::sound) {
        auto* paramsTree = paramsEl->createNewChildElement("PARAMETERS");
        for (int i = 0; i < VstEngineAudioProcessor::numSoundParameterIds; ++i) {
            const auto child = state.getChildWithProperty(
                "id", juce::var(VstEngineAudioProcessor::soundParameterIds[i]));
            if (child.isValid())
                paramsTree->addChildElement(child.createXml().release());
        }
    } else {
        paramsEl->addChildElement(state.createXml().release());
        xml.createNewChildElement("sequence")->setText(encodeSequence(sequence));
    }
}

bool PresetManager::deserializeFromXml(const juce::XmlElement& sourceXml,
                                       const PresetKind expectedKind)
{
    if (!sourceXml.hasTagName("VstEnginePreset"))
        return false;

    // Version dispatch / migration: rewrite the preset to the current schema
    // or reject it cleanly (unsupported future version, missing version).
    juce::XmlElement migrated("VstEnginePreset");
    if (!migrateToCurrentVersion(sourceXml, migrated))
        return false;
    const juce::XmlElement& xml = migrated;

    const auto typeAttr = xml.getStringAttribute("type");
    if (typeAttr != "sound" && typeAttr != "full")
        return false;
    if ((expectedKind == PresetKind::sound) != (typeAttr == "sound"))
        return false; // preset kind mismatch: fail safely

    // Parse everything into locals first; live plugin state is only touched
    // once the whole preset has been validated.
    juce::String newCurrentName = currentPresetName;
    if (auto* nameEl = xml.getChildByName("name")) {
        const auto n = nameEl->getAllSubText();
        if (n.isNotEmpty())
            newCurrentName = n;
    }

    // Full presets carry the sequence; Sound presets never touch it.
    vstengine::generator::Sequence restoredSequence;
    bool hasSequence = false;
    if (typeAttr == "full") {
        auto* seqEl = xml.getChildByName("sequence");
        if (seqEl == nullptr)
            return false;
        if (!decodeSequence(seqEl->getAllSubText(), restoredSequence))
            return false; // corrupt sequence: fail safely
        hasSequence = true;
    }

    auto* paramsEl = xml.getChildByName("parameters");
    if (paramsEl == nullptr)
        return false;
    auto* stateEl = paramsEl->getChildByName("PARAMETERS");
    if (stateEl == nullptr)
        return false;
    const juce::ValueTree restored = juce::ValueTree::fromXml(*stateEl);
    if (!restored.isValid())
        return false;

    if (typeAttr == "sound") {
        // Apply only the engine sound parameters onto the current state;
        // sequence, MIDI and generator settings are left untouched.
        auto newState = processor.parameters().copyState();
        for (int i = 0; i < VstEngineAudioProcessor::numSoundParameterIds; ++i) {
            const auto id = juce::var(VstEngineAudioProcessor::soundParameterIds[i]);
            const auto src = restored.getChildWithProperty("id", id);
            if (!src.isValid())
                continue;
            auto dst = newState.getChildWithProperty("id", id);
            if (dst.isValid())
                dst.copyPropertiesFrom(src, nullptr);
        }
        processor.parameters().replaceState(std::move(newState));
    } else {
        // Full preset: complete APVTS state + sequence.
        processor.parameters().replaceState(restored);
        sequence = restoredSequence;
    }

    currentPresetName = newCurrentName;
    return true;
}

void PresetManager::saveSoundPreset(const juce::String& name)
{
    currentPresetName = name;
    juce::XmlElement xml("VstEnginePreset");
    serializeToXml(xml, PresetKind::sound);
    writePresetFile(xml, name);
}

void PresetManager::saveFullPreset(const juce::String& name)
{
    currentPresetName = name;
    juce::XmlElement xml("VstEnginePreset");
    serializeToXml(xml, PresetKind::full);
    writePresetFile(xml, name);
}

void PresetManager::writePresetFile(const juce::XmlElement& xml,
                                    const juce::String& name)
{
    const auto safeName = makeSafeFilename(name);
    juce::File file(presetDirectory.getChildFile(safeName + ".xml"));
    if (file.existsAsFile())
        file.deleteFile();
    if (auto stream = file.createOutputStream()) {
        const juce::String xmlString = xml.toString();
        stream->write(xmlString.toRawUTF8(), xmlString.getNumBytesAsUTF8());
        stream->flush();
        currentPresetName = name;
    }
}

bool PresetManager::loadSoundPreset(const juce::String& name) {
    const auto safeName = makeSafeFilename(name);
    juce::File file(presetDirectory.getChildFile(safeName + ".xml"));
    if (!file.existsAsFile()) return false;
    const juce::String content = file.loadFileAsString();
    if (content.isEmpty()) return false;
    std::unique_ptr<juce::XmlElement> xml(juce::parseXML(content));
    if (!xml || !xml->hasTagName("VstEnginePreset")) return false;
    return deserializeFromXml(*xml, PresetKind::sound);
}

bool PresetManager::loadFullPreset(const juce::String& name) {
    const auto safeName = makeSafeFilename(name);
    juce::File file(presetDirectory.getChildFile(safeName + ".xml"));
    if (!file.existsAsFile()) return false;
    const juce::String content = file.loadFileAsString();
    if (content.isEmpty()) return false;
    std::unique_ptr<juce::XmlElement> xml(juce::parseXML(content));
    if (!xml || !xml->hasTagName("VstEnginePreset")) return false;
    return deserializeFromXml(*xml, PresetKind::full);
}

juce::StringArray PresetManager::getSoundPresetNames() const
{
    juce::StringArray names;
    if (!presetDirectory.exists()) return names;
    for (const auto& file : presetDirectory.findChildFiles(
             juce::File::findFiles, false, "*.xml")) {
        if (auto xml = std::unique_ptr<juce::XmlElement>(
                juce::parseXML(file.loadFileAsString()))) {
            if (xml->hasTagName("VstEnginePreset")
                && xml->getStringAttribute("type", "sound") == "sound") {
                if (auto* nameEl = xml->getChildByName("name")) {
                    const juce::String presetName = nameEl->getAllSubText();
                    if (!presetName.isEmpty())
                        names.addIfNotAlreadyThere(presetName);
                }
            }
        }
    }
    return names;
}

juce::StringArray PresetManager::getFullPresetNames() const
{
    juce::StringArray names;
    if (!presetDirectory.exists()) return names;
    for (const auto& file : presetDirectory.findChildFiles(
             juce::File::findFiles, false, "*.xml")) {
        if (auto xml = std::unique_ptr<juce::XmlElement>(
                juce::parseXML(file.loadFileAsString()))) {
            if (xml->hasTagName("VstEnginePreset")
                && xml->getStringAttribute("type", "sound") == "full") {
                if (auto* nameEl = xml->getChildByName("name")) {
                    const juce::String presetName = nameEl->getAllSubText();
                    if (!presetName.isEmpty())
                        names.addIfNotAlreadyThere(presetName);
                }
            }
        }
    }
    return names;
}

bool PresetManager::renamePreset(const juce::String& oldName, const juce::String& newName) {
    const auto safeOld = makeSafeFilename(oldName);
    const auto safeNew = makeSafeFilename(newName);
    juce::File oldFile(presetDirectory.getChildFile(safeOld + ".xml"));
    juce::File newFile(presetDirectory.getChildFile(safeNew + ".xml"));
    if (!oldFile.existsAsFile() || newFile.existsAsFile()) return false;
    if (oldFile.moveFileTo(newFile)) {
        currentPresetName = newName;
        return true;
    }
    return false;
}

bool PresetManager::deletePreset(const juce::String& name) {
    const auto safeName = makeSafeFilename(name);
    juce::File file(presetDirectory.getChildFile(safeName + ".xml"));
    if (!file.existsAsFile()) return false;
    const bool deleted = file.deleteFile();
    if (deleted && currentPresetName == name) currentPresetName.clear();
    return deleted;
}

juce::StringArray PresetManager::getFactoryPresetNames() const {
    juce::StringArray names;
    for (const auto& preset : factoryPresets)
        names.add(preset.name);
    return names;
}

bool PresetManager::loadFactoryPreset(const juce::String& name) {
    for (const auto& preset : factoryPresets) {
        if (name != preset.name)
            continue;

        // Build a v1 sound preset from the plain parameter values. Values are
        // normalized with each parameter's own range so the resulting XML is
        // identical in format to a user-saved Sound preset.
        juce::XmlElement xml("VstEnginePreset");
        xml.setAttribute("version", currentPresetVersion);
        xml.setAttribute("type", "sound");
        xml.createNewChildElement("name")->setText(preset.name);
        auto* paramsEl = xml.createNewChildElement("parameters");
        auto* paramsTree = paramsEl->createNewChildElement("PARAMETERS");

        for (const auto& param : preset.params) {
            auto* ranged = processor.parameters().getParameter(param.id);
            if (ranged == nullptr)
                return false;
            auto* paramEl = paramsTree->createNewChildElement("PARAM");
            paramEl->setAttribute("id", param.id);
            paramEl->setAttribute("value", juce::String(
                ranged->convertTo0to1(param.value), 8));
        }

        return deserializeFromXml(xml, PresetKind::sound);
    }
    return false;
}

} // namespace vstengine
