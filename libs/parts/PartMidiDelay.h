#pragma once

#include <array>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>

namespace vstengine::parts {

class PartMidiDelay final {
public:
    static constexpr int maxPendingEvents = 256;

    void reset() noexcept;
    void process(const juce::MidiBuffer& input, juce::MidiBuffer& output,
                 int blockSamples, int delaySamples) noexcept;
    [[nodiscard]] std::uint64_t overflowCount() const noexcept
    {
        return overflows_;
    }

private:
    struct PendingNote {
        int remainingSamples {};
        std::uint8_t channel { 1 };
        std::uint8_t note {};
        std::uint8_t velocity {};
        bool noteOn {};
    };

    [[nodiscard]] static juce::MidiMessage makeMessage(
        const PendingNote&) noexcept;
    void flush(juce::MidiBuffer&, int samplePosition) noexcept;

    std::array<PendingNote, maxPendingEvents> pending_ {};
    int pendingCount_ {};
    std::uint64_t overflows_ {};
};

} // namespace vstengine::parts
