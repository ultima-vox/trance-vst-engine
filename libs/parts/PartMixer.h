#pragma once

#include "parts/PartRegistry.h"
#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace vstengine::parts {

class PartMixer final {
public:
    using Inputs = std::array<const juce::AudioBuffer<float>*,
                              PartRegistry::capacity>;

    [[nodiscard]] static bool isAudible(const Part& part,
                                        bool anySolo) noexcept;
    static void mix(const PartRegistry& registry, const Inputs& inputs,
                    juce::AudioBuffer<float>& output, int numSamples) noexcept;
};

} // namespace vstengine::parts
