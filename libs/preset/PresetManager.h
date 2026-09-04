#pragma once
#include <JuceHeader.h>
#include "../generator/Sequence.h"

class VstEngineAudioProcessor;

namespace vstengine {

class PresetManager final {
public:
    // Explicit preset schema version. Bump when the on-disk format changes and
    // add a migrator in PresetManager.cpp (migrateToCurrentVersion).
    static constexpr int currentPresetVersion = 1;

    enum class PresetKind { sound, full };

    PresetManager(VstEngineAudioProcessor& processor,
                  vstengine::generator::Sequence& seq);

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
    VstEngineAudioProcessor& processor;
    vstengine::generator::Sequence& sequence;
    juce::File presetDirectory;
    juce::String currentPresetName;

    static juce::File getPresetDirectory();
    void serializeToXml(juce::XmlElement& xml, PresetKind kind);
    bool deserializeFromXml(const juce::XmlElement& xml, PresetKind expectedKind);
    void writePresetFile(const juce::XmlElement& xml, const juce::String& name);
    juce::String makeSafeFilename(const juce::String& name) const;
};

} // namespace vstengine
