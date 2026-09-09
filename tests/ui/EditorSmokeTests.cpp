#include "PluginEditor.h"
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

    editor->setSize(1040, 680);
    editor->setSize(1180, 760);
    editor->setSize(1500, 920);
    std::cout << "Editor smoke test passed\n";
    return EXIT_SUCCESS;
}
