#pragma once
#include "common/UiComponents.h"

namespace vstengine::ui {

class BassPanel final : public juce::Component {
public:
    explicit BassPanel (juce::AudioProcessorValueTreeState&);
    void resized() override;

private:
    ParameterSection filter { "FILTER" };
    ParameterSection amp { "AMP" };
    ParameterSection pitch { "PITCH" };
    ParameterSection output { "OUTPUT" };
};

} // namespace vstengine::ui
