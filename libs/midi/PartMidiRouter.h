#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

namespace vstengine::midi {

class PartMidiRouter final {
public:
    static constexpr int maxPendingEvents = 256;
    static constexpr int maxKickActionsPerBlock = 32;

    enum class KickActionType { trigger, panic };
    struct KickAction {
        KickActionType type { KickActionType::trigger };
        int samplePosition {};
        int note { 60 };
        float velocity { 1.0f };
    };

    void reset() noexcept;

    // Routes one block. Kick notes are consumed; ordinary kick note-offs are
    // intentionally ignored because kick is one-shot. Only kick-channel
    // CC120/123 create panic actions. Bass-channel notes receive fixed delay.
    // Overflow policy: flush all queued bass events at the current event's
    // position, then pass that event through. This degrades to zero delay but
    // preserves note ordering/pairs, so overflow cannot create a stuck note.
    void process(const juce::MidiBuffer& input, juce::MidiBuffer& output,
                 int blockSamples, int bassChannel, int kickChannel,
                 int bassDelaySamples) noexcept;

    [[nodiscard]] int numKickActions() const noexcept { return actionCount; }
    [[nodiscard]] const KickAction& kickAction(int index) const noexcept
    {
        return actions[static_cast<std::size_t>(index)];
    }
    [[nodiscard]] std::uint64_t overflowCount() const noexcept
    {
        return overflows;
    }

private:
    struct PendingNote {
        int remainingSamples {};
        std::uint8_t channel { 1 };
        std::uint8_t note {};
        std::uint8_t velocity {};
        bool noteOn {};
    };

    static juce::MidiMessage makeMessage(const PendingNote& event) noexcept;
    void flushPending(juce::MidiBuffer& output, int samplePosition) noexcept;
    void pushKickAction(KickAction action) noexcept;

    std::array<PendingNote, maxPendingEvents> pending {};
    int pendingCount {};
    std::array<KickAction, maxKickActionsPerBlock> actions {};
    int actionCount {};
    std::uint64_t overflows {};
};

} // namespace vstengine::midi
