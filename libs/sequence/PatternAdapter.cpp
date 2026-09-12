#include "sequence/PatternAdapter.h"
#include <bit>
#include <cmath>

namespace vstengine::sequence {
namespace {
constexpr std::uint32_t magic = 0x54415056u; // VPAT, little-endian.
void put16(std::byte*& out, std::uint16_t value) noexcept
{
    *out++ = static_cast<std::byte>(value & 0xffu);
    *out++ = static_cast<std::byte>((value >> 8) & 0xffu);
}
void put32(std::byte*& out, std::uint32_t value) noexcept
{
    for (int shift = 0; shift < 32; shift += 8)
        *out++ = static_cast<std::byte>((value >> shift) & 0xffu);
}
std::uint16_t get16(const std::byte*& in) noexcept
{
    const auto value = std::to_integer<std::uint16_t>(in[0])
        | (std::to_integer<std::uint16_t>(in[1]) << 8);
    in += 2;
    return value;
}
std::uint32_t get32(const std::byte*& in) noexcept
{
    const auto value = std::to_integer<std::uint32_t>(in[0])
        | (std::to_integer<std::uint32_t>(in[1]) << 8)
        | (std::to_integer<std::uint32_t>(in[2]) << 16)
        | (std::to_integer<std::uint32_t>(in[3]) << 24);
    in += 4;
    return value;
}
} // namespace

bool validPattern(const VoxPatternV1& pattern) noexcept
{
    if (pattern.structSize != sizeof(VoxPatternV1)
        || pattern.schemaVersion != VOX_PATTERN_SCHEMA_V1
        || pattern.stepCount == 0 || pattern.stepCount > VOX_PATTERN_MAX_STEPS
        || pattern.timingMode > static_cast<std::uint32_t>(TimingMode::triplet))
        return false;
    for (std::uint32_t i = 0; i < pattern.stepCount; ++i) {
        const auto& step = pattern.steps[i];
        if (step.noteOffset < -96 || step.noteOffset > 96
            || step.gate > 1 || step.accent > 1
            || !std::isfinite(step.velocity) || step.velocity < 0.0f
            || step.velocity > 1.0f || !std::isfinite(step.probability)
            || step.probability < 0.0f || step.probability > 1.0f
            || step.ratchetCount < 1 || step.ratchetCount > 8
            || !std::isfinite(step.slideDuration) || step.slideDuration < 0.0f
            || step.slideDuration > 8.0f || !std::isfinite(step.gateWidth)
            || step.gateWidth < 0.0f || step.gateWidth > 1.0f)
            return false;
    }
    return true;
}

VoxPatternV1 toPattern(const Sequence& source) noexcept
{
    VoxPatternV1 result {};
    result.structSize = sizeof(result);
    result.schemaVersion = VOX_PATTERN_SCHEMA_V1;
    result.stepCount = static_cast<std::uint32_t>(source.size());
    result.timingMode = static_cast<std::uint32_t>(source.getTimingMode());
    for (std::uint32_t i = 0; i < result.stepCount; ++i) {
        const auto& from = source[static_cast<int>(i)];
        auto& to = result.steps[i];
        to.noteOffset = static_cast<std::int16_t>(from.noteOffset);
        to.gate = from.gate ? 1 : 0;
        to.accent = from.accent ? 1 : 0;
        to.velocity = from.velocity;
        to.probability = from.probability;
        to.ratchetCount = static_cast<std::uint8_t>(from.ratchetCount);
        to.slideDuration = from.slideDuration;
        to.gateWidth = from.gateWidth;
    }
    return result;
}

bool fromPattern(const VoxPatternV1& source, Sequence& destination) noexcept
{
    if (!validPattern(source)) return false;
    Sequence candidate(static_cast<int>(source.stepCount));
    candidate.setTimingMode(static_cast<TimingMode>(source.timingMode));
    for (std::uint32_t i = 0; i < source.stepCount; ++i) {
        const auto& from = source.steps[i];
        auto& to = candidate[static_cast<int>(i)];
        to.noteOffset = from.noteOffset;
        to.gate = from.gate != 0;
        to.accent = from.accent != 0;
        to.velocity = from.velocity;
        to.probability = from.probability;
        to.ratchetCount = from.ratchetCount;
        to.slideDuration = from.slideDuration;
        to.gateWidth = from.gateWidth;
    }
    destination = candidate;
    return true;
}

bool encodePattern(const VoxPatternV1& pattern,
                   std::vector<std::byte>& destination)
{
    if (!validPattern(pattern)) return false;
    std::vector<std::byte> encoded(
        16u + static_cast<std::size_t>(pattern.stepCount) * 24u);
    auto* out = encoded.data();
    put32(out, magic);
    put32(out, pattern.schemaVersion);
    put32(out, pattern.stepCount);
    put32(out, pattern.timingMode);
    for (std::uint32_t i = 0; i < pattern.stepCount; ++i) {
        const auto& step = pattern.steps[i];
        put16(out, static_cast<std::uint16_t>(step.noteOffset));
        *out++ = static_cast<std::byte>(step.gate);
        *out++ = static_cast<std::byte>(step.accent);
        put32(out, std::bit_cast<std::uint32_t>(step.velocity));
        put32(out, std::bit_cast<std::uint32_t>(step.probability));
        *out++ = static_cast<std::byte>(step.ratchetCount);
        *out++ = std::byte {};
        *out++ = std::byte {};
        *out++ = std::byte {};
        put32(out, std::bit_cast<std::uint32_t>(step.slideDuration));
        put32(out, std::bit_cast<std::uint32_t>(step.gateWidth));
    }
    destination = std::move(encoded);
    return true;
}

bool decodePattern(std::span<const std::byte> payload,
                   VoxPatternV1& destination) noexcept
{
    if (payload.size() < 16 || payload.size() > maximumEncodedPatternBytes)
        return false;
    const auto* in = payload.data();
    if (get32(in) != magic) return false;
    VoxPatternV1 candidate {};
    candidate.structSize = sizeof(candidate);
    candidate.schemaVersion = get32(in);
    candidate.stepCount = get32(in);
    candidate.timingMode = get32(in);
    const auto expected = 16u + static_cast<std::size_t>(candidate.stepCount) * 24u;
    if (candidate.stepCount > VOX_PATTERN_MAX_STEPS || payload.size() != expected)
        return false;
    for (std::uint32_t i = 0; i < candidate.stepCount; ++i) {
        auto& step = candidate.steps[i];
        step.noteOffset = static_cast<std::int16_t>(get16(in));
        step.gate = std::to_integer<std::uint8_t>(*in++);
        step.accent = std::to_integer<std::uint8_t>(*in++);
        step.velocity = std::bit_cast<float>(get32(in));
        step.probability = std::bit_cast<float>(get32(in));
        step.ratchetCount = std::to_integer<std::uint8_t>(*in++);
        in += 3;
        step.slideDuration = std::bit_cast<float>(get32(in));
        step.gateWidth = std::bit_cast<float>(get32(in));
    }
    if (!validPattern(candidate)) return false;
    destination = candidate;
    return true;
}
} // namespace vstengine::sequence
