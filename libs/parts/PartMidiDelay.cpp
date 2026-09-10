#include "parts/PartMidiDelay.h"

namespace vstengine::parts {

juce::MidiMessage PartMidiDelay::makeMessage(const PendingNote& event) noexcept
{
    return event.noteOn
        ? juce::MidiMessage::noteOn(event.channel, event.note, event.velocity)
        : juce::MidiMessage::noteOff(event.channel, event.note, event.velocity);
}

void PartMidiDelay::reset() noexcept
{
    pendingCount_ = 0;
    overflows_ = 0;
}

void PartMidiDelay::flush(juce::MidiBuffer& output,
                          const int samplePosition) noexcept
{
    for (int i = 0; i < pendingCount_; ++i)
        output.addEvent(makeMessage(pending_[static_cast<std::size_t>(i)]),
                        samplePosition);
    pendingCount_ = 0;
}

void PartMidiDelay::process(const juce::MidiBuffer& input,
                            juce::MidiBuffer& output, const int blockSamples,
                            const int delaySamples) noexcept
{
    output.clear();
    const int safeBlock = juce::jmax(1, blockSamples);
    const int safeDelay = juce::jmax(0, delaySamples);

    for (const auto metadata : input) {
        const auto message = metadata.getMessage();
        if (message.isController()
            && (message.getControllerNumber() == 120
                || message.getControllerNumber() == 123)) {
            pendingCount_ = 0;
            break;
        }
    }

    int write = 0;
    for (int i = 0; i < pendingCount_; ++i) {
        auto event = pending_[static_cast<std::size_t>(i)];
        if (event.remainingSamples < safeBlock)
            output.addEvent(makeMessage(event), event.remainingSamples);
        else {
            event.remainingSamples -= safeBlock;
            pending_[static_cast<std::size_t>(write++)] = event;
        }
    }
    pendingCount_ = write;

    for (const auto metadata : input) {
        const auto message = metadata.getMessage();
        if (message.isController()
            && (message.getControllerNumber() == 120
                || message.getControllerNumber() == 123)) {
            pendingCount_ = 0;
            output.addEvent(message, metadata.samplePosition);
            continue;
        }
        if (safeDelay == 0 || !message.isNoteOnOrOff()) {
            output.addEvent(message, metadata.samplePosition);
            continue;
        }

        const int target = metadata.samplePosition + safeDelay;
        if (target < safeBlock) {
            output.addEvent(message, target);
        } else if (pendingCount_ < maxPendingEvents) {
            const auto* raw = message.getRawData();
            pending_[static_cast<std::size_t>(pendingCount_++)] = {
                target - safeBlock,
                static_cast<std::uint8_t>(message.getChannel()),
                static_cast<std::uint8_t>(message.getNoteNumber()),
                static_cast<std::uint8_t>(message.getRawDataSize() > 2
                                              ? raw[2] : 0),
                message.isNoteOn(false)
            };
        } else {
            ++overflows_;
            flush(output, metadata.samplePosition);
            output.addEvent(message, metadata.samplePosition);
        }
    }
}

} // namespace vstengine::parts
