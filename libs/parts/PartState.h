#pragma once

#include "parts/PartRegistry.h"
#include <juce_data_structures/juce_data_structures.h>

namespace vstengine::parts {

class PartState final {
public:
    static constexpr int currentVersion = 2;

    [[nodiscard]] static juce::ValueTree serialize(const PartRegistry& registry);
    // Transactional: registry remains unchanged on corrupt/unsupported state.
    [[nodiscard]] static bool restore(const juce::ValueTree& state,
                                      PartRegistry& registry);
};

} // namespace vstengine::parts
