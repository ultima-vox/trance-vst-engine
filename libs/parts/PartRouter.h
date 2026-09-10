#pragma once

#include "parts/PartRegistry.h"
#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace vstengine::parts {

class PartMidiBuffers final {
public:
    void prepare(int bytesPerPart);
    void clear() noexcept;

    [[nodiscard]] juce::MidiBuffer& operator[](std::size_t index) noexcept
    {
        return buffers_[index];
    }

    [[nodiscard]] const juce::MidiBuffer& operator[](std::size_t index) const noexcept
    {
        return buffers_[index];
    }

    [[nodiscard]] juce::MidiBuffer* find(const PartRegistry& registry,
                                         PartId id) noexcept;
    [[nodiscard]] const juce::MidiBuffer* find(const PartRegistry& registry,
                                               PartId id) const noexcept;

private:
    std::array<juce::MidiBuffer, PartRegistry::capacity> buffers_;
};

class PartRouter final {
public:
    void route(const juce::MidiBuffer& input, const PartRegistry& registry,
               PartMidiBuffers& output) const noexcept;

    // Explicit audition destination. Input channel is rewritten to Part channel.
    [[nodiscard]] bool routeToPart(const juce::MidiMessage& message,
                                   int samplePosition, PartId destination,
                                   const PartRegistry& registry,
                                   PartMidiBuffers& output) const noexcept;
};

} // namespace vstengine::parts
