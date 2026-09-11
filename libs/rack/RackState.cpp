#include "rack/RackState.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <unordered_set>

namespace vstengine::rack::state {
namespace {
constexpr auto rackType = "RACK";
constexpr auto slotType = "SLOT";
bool validString(const juce::var& value, std::size_t maxBytes = 256)
{
    return value.toString().getNumBytesAsUTF8() <= static_cast<int>(maxBytes);
}
bool validStableId(std::string_view id, bool allowEmpty = false)
{
    if (id.empty()) return allowEmpty;
    if (id.size() >= VOX_INSTRUMENT_MAX_ID_BYTES) return false;
    return std::all_of(id.begin(), id.end(), [](const unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '.' || c == '-' || c == '_';
    });
}
template <typename UInt>
bool readUnsigned(const juce::var& value, UInt& result)
{
    static_assert(std::is_unsigned_v<UInt>);
    const auto text = value.toString().toStdString();
    if (text.empty()) return false;
    std::uint64_t parsed {};
    const auto converted = std::from_chars(text.data(), text.data() + text.size(),
                                           parsed, 10);
    if (converted.ec != std::errc {} || converted.ptr != text.data() + text.size()
        || parsed > static_cast<std::uint64_t>(std::numeric_limits<UInt>::max()))
        return false;
    result = static_cast<UInt>(parsed);
    return true;
}
} // namespace

juce::ValueTree serialize(
    const std::array<PersistentSlotState, instrument::maxSlots>& slots)
{
    juce::ValueTree root(rackType);
    root.setProperty("schemaVersion", schemaVersion, nullptr);
    for (const auto& slot : slots) {
        juce::ValueTree child(slotType);
        child.setProperty("schemaVersion", static_cast<juce::int64>(slot.schemaVersion), nullptr);
        child.setProperty("slotId", juce::String(slot.slotId), nullptr);
        child.setProperty("instrumentId", juce::String(slot.instrumentId), nullptr);
        child.setProperty("providerId", juce::String(slot.resolvedProviderId), nullptr);
        child.setProperty("instrumentVersion", static_cast<juce::int64>(slot.instrumentVersion), nullptr);
        child.setProperty("instrumentStateVersion", static_cast<juce::int64>(slot.instrumentStateVersion), nullptr);
        child.setProperty("contentVersion", static_cast<juce::int64>(slot.contentVersion), nullptr);
        child.setProperty("routeMode", static_cast<int>(slot.routing.mode), nullptr);
        child.setProperty("channel", static_cast<int>(slot.routing.channel), nullptr);
        child.setProperty("keyLow", static_cast<int>(slot.routing.keyLow), nullptr);
        child.setProperty("keyHigh", static_cast<int>(slot.routing.keyHigh), nullptr);
        child.setProperty("velocityLow", static_cast<int>(slot.routing.velocityLow), nullptr);
        child.setProperty("velocityHigh", static_cast<int>(slot.routing.velocityHigh), nullptr);
        child.setProperty("transpose", static_cast<int>(slot.routing.transpose), nullptr);
        child.setProperty("enabled", slot.enabled, nullptr);
        child.setProperty("mute", slot.mute, nullptr);
        child.setProperty("solo", slot.solo, nullptr);
        child.setProperty("locked", slot.locked, nullptr);
        child.setProperty("level", slot.level, nullptr);
        child.setProperty("pan", slot.pan, nullptr);
        child.setProperty("output", static_cast<int>(slot.outputDestination), nullptr);
        child.setProperty("soundPreset", juce::String(slot.soundPreset), nullptr);
        child.setProperty("patternPreset", juce::String(slot.patternPreset), nullptr);
        for (std::size_t i = 0; i < slot.macros.size(); ++i) {
            child.setProperty("macro" + juce::String(static_cast<int>(i + 1)),
                              slot.macros[i], nullptr);
            child.setProperty("macroMap" + juce::String(static_cast<int>(i + 1)),
                              juce::String(slot.macroAssignments[i]), nullptr);
        }
        juce::MemoryBlock payload(slot.modulePayload.data(), slot.modulePayload.size());
        child.setProperty("modulePayload", payload.toBase64Encoding(), nullptr);
        root.appendChild(child, nullptr);
    }
    return root;
}

bool deserialize(const juce::ValueTree& root,
                 std::array<PersistentSlotState, instrument::maxSlots>& result,
                 std::string& diagnostic)
{
    std::uint32_t encodedSchema {};
    if (!root.isValid() || !root.hasType(rackType)
        || !readUnsigned(root.getProperty("schemaVersion"), encodedSchema)
        || encodedSchema != static_cast<std::uint32_t>(schemaVersion)
        || root.getNumChildren() != static_cast<int>(instrument::maxSlots)) {
        diagnostic = "invalid rack state root";
        return false;
    }
    std::array<PersistentSlotState, instrument::maxSlots> parsed;
    std::unordered_set<instrument::SlotId> ids;
    for (int i = 0; i < root.getNumChildren(); ++i) {
        const auto child = root.getChild(i);
        auto& slot = parsed[static_cast<std::size_t>(i)];
        if (!child.hasType(slotType)
            || !validString(child.getProperty("instrumentId"))
            || !validString(child.getProperty("providerId"))
            || !validString(child.getProperty("soundPreset"))
            || !validString(child.getProperty("patternPreset"))) {
            diagnostic = "invalid rack slot encoding";
            return false;
        }
        if (!readUnsigned(child.getProperty("schemaVersion"), slot.schemaVersion)
            || !readUnsigned(child.getProperty("slotId"), slot.slotId)
            || !readUnsigned(child.getProperty("instrumentVersion"),
                             slot.instrumentVersion)
            || !readUnsigned(child.getProperty("instrumentStateVersion"),
                             slot.instrumentStateVersion)
            || !readUnsigned(child.getProperty("contentVersion"),
                             slot.contentVersion)) {
            diagnostic = "invalid rack unsigned metadata";
            return false;
        }
        slot.instrumentId = child.getProperty("instrumentId").toString().toStdString();
        slot.resolvedProviderId = child.getProperty("providerId").toString().toStdString();
        if (!validStableId(slot.instrumentId, true)
            || !validStableId(slot.resolvedProviderId, true)) {
            diagnostic = "invalid rack stable ID";
            return false;
        }
        slot.routing.mode = static_cast<RouteMode>(
            static_cast<int>(child.getProperty("routeMode", -1)));
        slot.routing.channel = static_cast<std::uint8_t>(static_cast<int>(child.getProperty("channel", 0)));
        slot.routing.keyLow = static_cast<std::uint8_t>(static_cast<int>(child.getProperty("keyLow", -1)));
        slot.routing.keyHigh = static_cast<std::uint8_t>(static_cast<int>(child.getProperty("keyHigh", -1)));
        slot.routing.velocityLow = static_cast<std::uint8_t>(static_cast<int>(child.getProperty("velocityLow", 0)));
        slot.routing.velocityHigh = static_cast<std::uint8_t>(static_cast<int>(child.getProperty("velocityHigh", 0)));
        slot.routing.transpose = static_cast<std::int8_t>(static_cast<int>(child.getProperty("transpose", 99)));
        slot.enabled = static_cast<bool>(child.getProperty("enabled", false));
        slot.mute = static_cast<bool>(child.getProperty("mute", false));
        slot.solo = static_cast<bool>(child.getProperty("solo", false));
        slot.locked = static_cast<bool>(child.getProperty("locked", false));
        slot.level = static_cast<float>(child.getProperty("level", 1.0));
        slot.pan = static_cast<float>(child.getProperty("pan", 0.0));
        if (!readUnsigned(child.getProperty("output"), slot.outputDestination)) {
            diagnostic = "invalid rack output metadata";
            return false;
        }
        slot.soundPreset = child.getProperty("soundPreset").toString().toStdString();
        slot.patternPreset = child.getProperty("patternPreset").toString().toStdString();
        for (std::size_t macro = 0; macro < slot.macros.size(); ++macro) {
            slot.macros[macro] = static_cast<float>(child.getProperty(
                "macro" + juce::String(static_cast<int>(macro + 1)), 0.0));
            slot.macroAssignments[macro] = child.getProperty(
                "macroMap" + juce::String(static_cast<int>(macro + 1))).toString().toStdString();
            if (!std::isfinite(slot.macros[macro]) || slot.macros[macro] < 0.0f
                || slot.macros[macro] > 1.0f
                || !validStableId(slot.macroAssignments[macro], true)) {
                diagnostic = "invalid rack macro state";
                return false;
            }
        }
        const auto encodedPayload = child.getProperty("modulePayload").toString();
        constexpr auto maxEncodedBytes =
            4u * ((maxModulePayloadBytes + 2u) / 3u);
        if (encodedPayload.getNumBytesAsUTF8()
            > static_cast<int>(maxEncodedBytes)) {
            diagnostic = "rack module payload exceeds host budget";
            return false;
        }
        juce::MemoryBlock payload;
        if (!payload.fromBase64Encoding(encodedPayload)) {
            diagnostic = "invalid rack module payload";
            return false;
        }
        if (payload.getSize() > maxModulePayloadBytes) {
            diagnostic = "rack module payload exceeds host budget";
            return false;
        }
        slot.modulePayload.resize(payload.getSize());
        if (!slot.modulePayload.empty())
            std::memcpy(slot.modulePayload.data(), payload.getData(), payload.getSize());
        if (!valid(slot.routing) || slot.schemaVersion != 1
            || slot.slotId == instrument::invalidSlotId
            || !ids.insert(slot.slotId).second || slot.outputDestination != 0) {
            diagnostic = "invalid or duplicate rack slot identity";
            return false;
        }
    }
    result = std::move(parsed);
    return true;
}
} // namespace vstengine::rack::state
