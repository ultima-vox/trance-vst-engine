#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace vstengine::ui {

class MainNavigation final : public juce::Component {
public:
    enum class Page {
        sound = 0, rack = sound, presets = sound,
        pattern = 1, sequence = pattern,
        routing = 2, zones = 3, macros = 4,
        advanced = 5, settings = advanced,
        count = 6
    };
    MainNavigation();
    void resized() override;
    void setCurrentPage (Page);
    [[nodiscard]] Page getCurrentPage() const noexcept { return currentPage; }
    std::function<void(Page)> onPageChanged;

private:
    std::array<juce::TextButton, static_cast<size_t> (Page::count)> buttons;
    Page currentPage { Page::rack };
};

} // namespace vstengine::ui
