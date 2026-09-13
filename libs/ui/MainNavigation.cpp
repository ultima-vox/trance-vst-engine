#include "MainNavigation.h"
#include "common/UiComponents.h"

namespace vstengine::ui {

MainNavigation::MainNavigation()
{
    static constexpr const char* names[] {
        "SOUND", "PATTERN", "ROUTING", "ZONES", "MACROS", "ADVANCED"
    };
    for (size_t i = 0; i < buttons.size(); ++i) {
        auto& button = buttons[i];
        button.setButtonText (names[i]);
        button.setClickingTogglesState (false);
        button.onClick = [this, i] { setCurrentPage (static_cast<Page> (i)); };
        styleButton (button);
        addAndMakeVisible (button);
    }
    setCurrentPage (Page::sound);
}

void MainNavigation::setCurrentPage (Page page)
{
    if (page < Page::sound || page >= Page::count)
        return;
    currentPage = page;
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].setToggleState (i == static_cast<size_t> (page), juce::dontSendNotification);
    if (onPageChanged)
        onPageChanged (page);
}

void MainNavigation::resized()
{
    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    row.justifyContent = juce::FlexBox::JustifyContent::center;
    for (auto& button : buttons)
        row.items.add (juce::FlexItem (button).withFlex (1.0f).withMargin (2.0f));
    row.performLayout (getLocalBounds());
}

} // namespace vstengine::ui
