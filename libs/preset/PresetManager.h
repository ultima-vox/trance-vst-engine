#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "sequence/Sequence.h"

namespace vstengine {

// Explicit parameter-state bridge between the preset module and the host
// integration layer (APVTS in the plugin shell). The preset module must not
// know about the plugin class, the editor or Cubase: everything it needs from
// the live plugin is expressed by this interface and passed in at
// construction. Keeps preset save/load unit-testable without a plugin
// instance and out of the realtime path by construction.
class PresetStateStore {
public:
    virtual ~PresetStateStore() = default;

    // Complete parameter state tree (APVTS copyState equivalent).
    [[nodiscard]] virtual juce::ValueTree copyState() const = 0;
    // Atomically install a new complete parameter state tree.
    virtual void replaceState(juce::ValueTree newState) = 0;
    // Convert a plain (denormalized) parameter value to the normalized form
    // used in preset XML, for the parameter with the given ID. Returns the
    // plain value unchanged when the ID is unknown.
    [[nodiscard]] virtual float convertTo0to1(const char* parameterId,
                                              float plainValue) const = 0;
};

class PresetManager final {
public:
    // Explicit preset schema version. Bump when the on-disk format changes and
    // add a migrator in PresetManager.cpp (migrateToCurrentVersion).
    static constexpr int currentPresetVersion = 1;

    enum class PresetKind { sound, full };

    PresetManager(PresetStateStore& stateStore,
                  vstengine::sequence::Sequence& seq,
                  juce::File directory = getPresetDirectory());

    void saveSoundPreset(const juce::String& name);
    void saveFullPreset(const juce::String& name);
    bool loadSoundPreset(const juce::String& name);
    bool loadFullPreset(const juce::String& name);
    juce::StringArray getSoundPresetNames() const;
    juce::StringArray getFullPresetNames() const;
    bool renamePreset(const juce::String& oldName, const juce::String& newName);
    bool deletePreset(const juce::String& name);
    juce::StringArray getFactoryPresetNames() const;
    bool loadFactoryPreset(const juce::String& name);
    juce::String getCurrentPresetName() const { return currentPresetName; }

private:
    PresetStateStore& store;
    vstengine::sequence::Sequence& sequence;
    juce::File presetDirectory;
    juce::String currentPresetName;

    static juce::File getPresetDirectory();
    void serializeToXml(juce::XmlElement& xml, PresetKind kind);
    bool deserializeFromXml(const juce::XmlElement& xml, PresetKind expectedKind);
    void writePresetFile(const juce::XmlElement& xml, const juce::String& name);
    juce::String makeSafeFilename(const juce::String& name) const;
};

} // namespace vstengine
