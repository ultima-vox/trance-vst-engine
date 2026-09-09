#pragma once
#include "common/UiComponents.h"
#include "preset/PresetManager.h"

namespace vstengine::ui {
class PresetBrowser final : public juce::Component, private juce::ListBoxModel {
public:
    explicit PresetBrowser (vstengine::PresetManager&,
                            std::function<void()> presetLoaded = {});
    void resized() override; void paint (juce::Graphics&) override;
    void refresh();
    [[nodiscard]] juce::String getStatusText() const { return status.getText(); }
private:
    using Entry = vstengine::PresetManager::PresetEntry;
    int getNumRows() override { return static_cast<int> (filtered.size()); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void selectedRowsChanged (int) override;
    void rebuildFilter(); void load(); void save (bool full); void rename(); void remove();
    vstengine::PresetManager& manager;
    std::function<void()> onPresetLoaded;
    vstengine::PresetManager::PresetSource source { vstengine::PresetManager::PresetSource::factory };
    std::vector<Entry> all, filtered;
    juce::TextButton factoryButton { "FACTORY" }, userButton { "USER" };
    juce::TextEditor search, name;
    juce::ComboBox category, soundEngine;
    juce::ListBox list { "Presets", this };
    juce::TextButton loadButton { "LOAD" }, saveButton { "SAVE" }, saveFullButton { "SAVE FULL" },
                     renameButton { "RENAME" }, deleteButton { "DELETE" }, refreshButton { "REFRESH" };
    juce::Label type, status;
};
} // namespace vstengine::ui
