#include "midi/PartMidiRouter.h"

namespace vstengine::midi {

juce::MidiMessage PartMidiRouter::makeMessage(
    const PendingNote& event) noexcept
{
    return event.noteOn
        ? juce::MidiMessage::noteOn(event.channel, event.note, event.velocity)
        : juce::MidiMessage::noteOff(event.channel, event.note, event.velocity);
}

void PartMidiRouter::flushPending(juce::MidiBuffer& output,
                                  const int samplePosition) noexcept
{
    for (int i = 0; i < pendingCount; ++i)
        output.addEvent(makeMessage(pending[static_cast<std::size_t>(i)]),
                        samplePosition);
    pendingCount = 0;
}

void PartMidiRouter::pushKickAction(KickAction action) noexcept
{
    if (actionCount < maxKickActionsPerBlock) {
        actions[static_cast<std::size_t>(actionCount++)] = action;
        return;
    }

    ++overflows;
    // Never discard safety action. Replacing final trigger preserves bounded
    // storage while guaranteeing dense MIDI cannot suppress panic.
    if (action.type == KickActionType::panic)
        actions.back() = action;
}

void PartMidiRouter::reset() noexcept
{
    pendingCount = 0;
    actionCount = 0;
    overflows = 0;
}

void PartMidiRouter::process(const juce::MidiBuffer& input,
                             juce::MidiBuffer& output,
                             const int blockSamples,
                             const int bassChannel,
                             const int kickChannel,
                             const int bassDelaySamples) noexcept
{
    output.clear();
    actionCount = 0;
    const int safeBlock = juce::jmax(1, blockSamples);
    const int safeBass = juce::jlimit(1, 16, bassChannel);
    const int safeKick = juce::jlimit(1, 16, kickChannel);
    const int safeDelay = juce::jmax(0, bassDelaySamples);

    int write = 0;
    for (int i = 0; i < pendingCount; ++i) {
        auto event = pending[static_cast<std::size_t>(i)];
        if (event.remainingSamples < safeBlock) {
            output.addEvent(makeMessage(event), event.remainingSamples);
        } else {
            event.remainingSamples -= safeBlock;
            pending[static_cast<std::size_t>(write++)] = event;
        }
    }
    pendingCount = write;

    for (const auto metadata : input) {
        const auto message = metadata.getMessage();
        const int channel = message.getChannel();

        if (channel == safeKick && message.isNoteOnOrOff()) {
            if (message.isNoteOn(false)) {
                pushKickAction({
                    KickActionType::trigger, metadata.samplePosition,
                    message.getNoteNumber(), message.getFloatVelocity()
                });
            }
            continue;
        }

        if (channel == safeKick && message.isController()
            && (message.getControllerNumber() == 120
                || message.getControllerNumber() == 123)) {
            pushKickAction({ KickActionType::panic, metadata.samplePosition,
                             0, 0.0f });
            continue;
        }

        if (channel == safeBass && safeDelay > 0 && message.isNoteOnOrOff()) {
            const int target = metadata.samplePosition + safeDelay;
            if (target < safeBlock) {
                output.addEvent(message, target);
            } else if (pendingCount < maxPendingEvents) {
                const auto* raw = message.getRawData();
                pending[static_cast<std::size_t>(pendingCount++)] = {
                    target - safeBlock,
                    static_cast<std::uint8_t>(channel),
                    static_cast<std::uint8_t>(message.getNoteNumber()),
                    static_cast<std::uint8_t>(message.getRawDataSize() > 2
                                                  ? raw[2] : 0),
                    message.isNoteOn(false)
                };
            } else {
                ++overflows;
                flushPending(output, metadata.samplePosition);
                output.addEvent(message, metadata.samplePosition);
            }
            continue;
        }

        output.addEvent(message, metadata.samplePosition);
    }
}

} // namespace vstengine::midi
