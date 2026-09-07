#include "part/PartState.h"
#include "sequence/Sequence.h"

namespace {
juce::String encodeSequence(const vstengine::sequence::Sequence& seq)
{
    juce::MemoryBlock mb;
    seq.serialize(mb);
    return mb.toBase64Encoding();
}

bool decodeSequence(const juce::String& encoded,
                    vstengine::sequence::Sequence& seq)
{
    juce::MemoryBlock mb;
    if (!mb.fromBase64Encoding(encoded))
        return false;
    if (!vstengine::sequence::Sequence::isValidSerialization(mb))
        return false;
    seq = vstengine::sequence::Sequence::deserialize(mb);
    return true;
}
} // namespace

namespace vstengine::part {

void writePartState(juce::ValueTree& tree, const Part& part, int index)
{
    auto child = juce::ValueTree("PART");
    child.setProperty("index", index, nullptr);
    child.setProperty("engine",
                      static_cast<int>(part.engine), nullptr);
    child.setProperty("midiChannel", part.midiChannel, nullptr);
    child.setProperty("mute", part.mute, nullptr);
    child.setProperty("solo", part.solo, nullptr);
    child.setProperty("lock", part.lock, nullptr);
    child.setProperty("level", part.level, nullptr);
    child.setProperty("pan", part.pan, nullptr);
    child.setProperty("sequence", encodeSequence(part.sequence), nullptr);
    tree.appendChild(child, nullptr);
}

bool readPartState(const juce::ValueTree& tree, Part& part, int index)
{
    for (int i = 0; i < tree.getNumChildren(); ++i) {
        const auto& child = tree.getChild(i);
        if (child.hasType("PART")
            && int(child.getProperty("index", 0)) == index) {
            part.engine = static_cast<Engine>(
                int(child.getProperty("engine", 0)));
            part.midiChannel =
                int(child.getProperty("midiChannel", index + 1));
            part.mute = bool(child.getProperty("mute", false));
            part.solo = bool(child.getProperty("solo", false));
            part.lock = bool(child.getProperty("lock", false));
            part.level = float(child.getProperty("level", 1.0f));
            part.pan = float(child.getProperty("pan", 0.0f));
            const auto seqStr =
                child.getProperty("sequence", juce::String()).toString();
            if (!seqStr.isEmpty())
                decodeSequence(seqStr, part.sequence);
            return true;
        }
    }
    return false;
}

void writePartArray(juce::ValueTree& tree, const PartArray& parts)
{
    for (int i = 0; i < parts.size(); ++i)
        writePartState(tree, parts[i], i);
}

} // namespace vstengine::part