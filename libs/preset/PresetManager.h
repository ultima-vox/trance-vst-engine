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
    enum class SoundEngine { none, bass, kick, legacyCombined };
    enum class PresetSource { factory, user };

    enum class Error {
        none,
        invalidName,
        fileNotFound,
        invalidPreset,
        unsupportedVersion,
        cannotWrite,
        alreadyExists,
        readOnly,
        wrongKind
    };

    struct OperationResult {
        Error error { Error::none };
        juce::String message;

        [[nodiscard]] bool wasOk() const noexcept { return error == Error::none; }
        explicit operator bool() const noexcept { return wasOk(); }
    };

    struct PresetEntry {
        juce::String name;
        PresetSource source { PresetSource::user };
        PresetKind kind { PresetKind::sound };
        SoundEngine engine { SoundEngine::none };
        juce::File file;

        [[nodiscard]] bool isReadOnly() const noexcept
        {
            return source == PresetSource::factory;
        }
    };

    PresetManager(PresetStateStore& stateStore,
                  vstengine::sequence::Sequence& seq,
                  juce::File directory = getPresetDirectory());

    OperationResult saveSoundPreset(const juce::String& name,
                                    SoundEngine engine = SoundEngine::bass);
    OperationResult saveFullPreset(const juce::String& name);
    OperationResult loadSoundPreset(const juce::String& name);
    OperationResult loadFullPreset(const juce::String& name);
    OperationResult loadPreset(const PresetEntry& entry);
    juce::StringArray getSoundPresetNames() const;
    juce::StringArray getFullPresetNames() const;
    juce::Array<PresetEntry> getPresets() const;
    OperationResult renamePreset(const juce::String& oldName,
                                 const juce::String& newName);
    OperationResult renamePreset(const PresetEntry& entry,
                                 const juce::String& newName);
    OperationResult deletePreset(const juce::String& name);
    OperationResult deletePreset(const PresetEntry& entry);
    juce::StringArray getFactoryPresetNames() const;
    OperationResult loadFactoryPreset(const juce::String& name);
    juce::String getCurrentPresetName() const { return currentPresetName; }

private:
    PresetStateStore& store;
    vstengine::sequence::Sequence& sequence;
    juce::File presetDirectory;
    juce::String currentPresetName;

    static juce::File getPresetDirectory();
    void serializeToXml(juce::XmlElement& xml, PresetKind kind,
                        SoundEngine engine = SoundEngine::none);
    bool deserializeFromXml(const juce::XmlElement& xml, PresetKind expectedKind);
    OperationResult writePresetFile(const juce::XmlElement& xml,
                                    const juce::String& name);
    OperationResult loadPresetFile(const juce::File& file,
                                   PresetKind expectedKind);
    OperationResult validateName(const juce::String& name) const;
    bool isFactoryName(const juce::String& name) const;
    juce::String makeSafeFilename(const juce::String& name) const;
};

} // namespace vstengine
