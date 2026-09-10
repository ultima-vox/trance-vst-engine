#include "parts/PartState.h"
#include <cmath>
#include <utility>

namespace {

juce::String encode(const vstengine::sequence::Sequence& sequence)
{
    juce::MemoryBlock data;
    sequence.serialize(data);
    return data.toBase64Encoding();
}

bool decode(const juce::var& value, vstengine::sequence::Sequence& sequence)
{
    juce::MemoryBlock data;
    if (!data.fromBase64Encoding(value.toString())
        || !vstengine::sequence::Sequence::isValidSerialization(data))
        return false;
    sequence = vstengine::sequence::Sequence::deserialize(data);
    return true;
}

bool readPart(const juce::ValueTree& node, const int version,
              vstengine::parts::Part& part)
{
    if (!node.hasType("PART"))
        return false;

    const int rawId = static_cast<int>(node.getProperty("id", 0));
    const int rawEngine = static_cast<int>(node.getProperty("engineType", 0));
    if (rawId != static_cast<int>(part.id)
        || rawEngine != static_cast<int>(part.engineType))
        return false;

    const int channel = static_cast<int>(node.getProperty("midiChannel", 0));
    const float level = static_cast<float>(node.getProperty("level", -1.0f));
    const float pan = static_cast<float>(node.getProperty("pan", 2.0f));
    if (channel < 1 || channel > 16 || !std::isfinite(level)
        || level < 0.0f || level > 2.0f || !std::isfinite(pan)
        || pan < -1.0f || pan > 1.0f || !node.hasProperty("sequence"))
        return false;

    vstengine::sequence::Sequence sequence;
    if (!decode(node.getProperty("sequence"), sequence))
        return false;

    part.midiChannel = channel;
    part.enabled = version >= 2
        ? static_cast<bool>(node.getProperty("enabled", true)) : true;
    part.mute = static_cast<bool>(node.getProperty("mute", false));
    part.solo = static_cast<bool>(node.getProperty("solo", false));
    part.level = level;
    part.pan = pan;
    part.locked = static_cast<bool>(node.getProperty("locked", false));
    part.sequence = std::move(sequence);
    return true;
}

} // namespace

namespace vstengine::parts {

juce::ValueTree PartState::serialize(const PartRegistry& registry)
{
    juce::ValueTree root("PARTS");
    root.setProperty("version", currentVersion, nullptr);
    for (std::size_t i = 0; i < registry.size(); ++i) {
        const auto& part = registry[i];
        juce::ValueTree node("PART");
        node.setProperty("id", static_cast<int>(part.id), nullptr);
        node.setProperty("engineType", static_cast<int>(part.engineType), nullptr);
        node.setProperty("midiChannel", part.midiChannel, nullptr);
        node.setProperty("enabled", part.enabled, nullptr);
        node.setProperty("mute", part.mute, nullptr);
        node.setProperty("solo", part.solo, nullptr);
        node.setProperty("level", part.level, nullptr);
        node.setProperty("pan", part.pan, nullptr);
        node.setProperty("locked", part.locked, nullptr);
        node.setProperty("sequence", encode(part.sequence), nullptr);
        root.appendChild(node, nullptr);
    }
    return root;
}

bool PartState::restore(const juce::ValueTree& state, PartRegistry& registry)
{
    if (!state.hasType("PARTS"))
        return false;
    const int version = static_cast<int>(state.getProperty("version", 0));
    if ((version != 1 && version != currentVersion)
        || state.getNumChildren() != static_cast<int>(PartRegistry::capacity))
        return false;

    PartRegistry candidate;
    std::array<bool, PartRegistry::capacity> seen {};
    for (int i = 0; i < state.getNumChildren(); ++i) {
        const auto node = state.getChild(i);
        const int rawId = static_cast<int>(node.getProperty("id", 0));
        const auto id = static_cast<PartId>(rawId);
        Part* destination = candidate.find(id);
        if (destination == nullptr)
            return false;
        const std::size_t slot = id == PartId::bass ? 0u : 1u;
        if (seen[slot] || !readPart(node, version, *destination))
            return false;
        seen[slot] = true;
    }

    if (!seen[0] || !seen[1] || !candidate.isValid())
        return false;
    registry = std::move(candidate);
    return true;
}

} // namespace vstengine::parts
