#include "PluginEditor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    VstEngineAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> base(processor.createEditor());
    auto* editor = dynamic_cast<VstEngineAudioProcessorEditor*>(base.get());
    if (editor == nullptr)
        return EXIT_FAILURE;

    using Page = vstengine::ui::MainNavigation::Page;
    for (const auto page : { Page::bass, Page::kick, Page::sequence,
                             Page::match, Page::presets, Page::settings }) {
        editor->showPageForTesting(page);
        if (editor->currentPageForTesting() != page)
            return EXIT_FAILURE;
    }

    for (const auto size : { juce::Point<int> { 1040, 680 },
                             juce::Point<int> { 1180, 760 },
                             juce::Point<int> { 1500, 920 } }) {
        editor->setSize(size.x, size.y);
        for (const auto page : { Page::bass, Page::kick }) {
            const auto coveredWidth = editor->keyboardKeyWidthForTesting(page)
                                      * 43.0f;
            const auto componentWidth = static_cast<float>(
                editor->keyboardComponentWidthForTesting(page));
            if (std::abs(coveredWidth - componentWidth) > 0.5f)
                return EXIT_FAILURE;
        }
    }
    std::cout << "Editor smoke test passed\n";
    return EXIT_SUCCESS;
}
