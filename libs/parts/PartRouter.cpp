#include "parts/PartRouter.h"

namespace vstengine::parts {

void PartMidiBuffers::prepare(const int bytesPerPart)
{
    for (auto& buffer : buffers_)
        buffer.ensureSize(juce::jmax(0, bytesPerPart));
}

void PartMidiBuffers::clear() noexcept
{
    for (auto& buffer : buffers_)
        buffer.clear();
}

juce::MidiBuffer* PartMidiBuffers::find(const PartRegistry& registry,
                                        const PartId id) noexcept
{
    for (std::size_t i = 0; i < registry.size(); ++i)
        if (registry[i].id == id)
            return &buffers_[i];
    return nullptr;
}

const juce::MidiBuffer* PartMidiBuffers::find(const PartRegistry& registry,
                                              const PartId id) const noexcept
{
    for (std::size_t i = 0; i < registry.size(); ++i)
        if (registry[i].id == id)
            return &buffers_[i];
    return nullptr;
}

void PartRouter::route(const juce::MidiBuffer& input,
                       const PartRegistry& registry,
                       PartMidiBuffers& output) const noexcept
{
    output.clear();
    for (const auto metadata : input) {
        const auto message = metadata.getMessage();
        const int channel = message.getChannel();
        if (channel < 1 || channel > 16)
            continue;

        for (std::size_t i = 0; i < registry.size(); ++i) {
            const auto& part = registry[i];
            if (part.enabled && part.midiChannel == channel)
                output[i].addEvent(message, metadata.samplePosition);
        }
    }
}

bool PartRouter::routeToPart(const juce::MidiMessage& message,
                             const int samplePosition,
                             const PartId destination,
                             const PartRegistry& registry,
                             PartMidiBuffers& output) const noexcept
{
    for (std::size_t i = 0; i < registry.size(); ++i) {
        const auto& part = registry[i];
        if (part.id != destination || !part.enabled)
            continue;
        auto routed = message;
        if (routed.getChannel() >= 1 && routed.getChannel() <= 16)
            routed.setChannel(part.midiChannel);
        output[i].addEvent(routed, juce::jmax(0, samplePosition));
        return true;
    }
    return false;
}

} // namespace vstengine::parts
