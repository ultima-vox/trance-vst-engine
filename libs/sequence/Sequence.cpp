#include "Sequence.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>

namespace vstengine::generator {

namespace {
    constexpr uint32_t magicNumber = 0x53514551; // "SEQQ"

    // --- Explicit fixed-width serialization helpers ---
    // All multi-byte values are written in little-endian byte order (consistent
    // on x86/x64 and documented for any future big-endian port).

    void writeU8(std::uint8_t* dst, std::uint8_t value) noexcept
    {
        dst[0] = value;
    }

    void writeU16(std::uint8_t* dst, std::uint16_t value) noexcept
    {
        dst[0] = static_cast<std::uint8_t>(value & 0xFF);
        dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    }

    void writeI32(std::uint8_t* dst, std::int32_t value) noexcept
    {
        const auto u = static_cast<std::uint32_t>(value);
        dst[0] = static_cast<std::uint8_t>(u & 0xFF);
        dst[1] = static_cast<std::uint8_t>((u >> 8) & 0xFF);
        dst[2] = static_cast<std::uint8_t>((u >> 16) & 0xFF);
        dst[3] = static_cast<std::uint8_t>((u >> 24) & 0xFF);
    }

    void writeU32(std::uint8_t* dst, std::uint32_t value) noexcept
    {
        dst[0] = static_cast<std::uint8_t>(value & 0xFF);
        dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        dst[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        dst[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }

    void writeFloat32(std::uint8_t* dst, float value) noexcept
    {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
        std::memcpy(&bits, &value, sizeof(value));
        writeU32(dst, bits);
    }

    std::uint8_t readU8(const std::uint8_t* src) noexcept
    {
        return src[0];
    }

    std::uint16_t readU16(const std::uint8_t* src) noexcept
    {
        return static_cast<std::uint16_t>(src[0])
             | (static_cast<std::uint16_t>(src[1]) << 8);
    }

    std::uint32_t readU32(const std::uint8_t* src) noexcept
    {
        return static_cast<std::uint32_t>(src[0])
             | (static_cast<std::uint32_t>(src[1]) << 8)
             | (static_cast<std::uint32_t>(src[2]) << 16)
             | (static_cast<std::uint32_t>(src[3]) << 24);
    }

    std::int32_t readI32(const std::uint8_t* src) noexcept
    {
        const auto u = readU32(src);
        return static_cast<std::int32_t>(u);
    }

    float readFloat32(const std::uint8_t* src) noexcept
    {
        const auto bits = static_cast<std::uint32_t>(src[0])
                        | (static_cast<std::uint32_t>(src[1]) << 8)
                        | (static_cast<std::uint32_t>(src[2]) << 16)
                        | (static_cast<std::uint32_t>(src[3]) << 24);
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    // Per-step binary footprint: fixed-width, no struct padding.
    // gate(1) + noteOffset(4) + velocity(4) + accent(1) + probability(4) +
    // ratchetCount(4) + slideDuration(4) + gateWidth(4) = 26 bytes/step
    constexpr size_t stepFieldBytes = 26;

    size_t headerBytes() noexcept
    {
        // magic(4) + version(2) + numSteps(4) + timingMode(1) = 11 bytes
        return sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::int32_t) + sizeof(std::uint8_t);
    }

    size_t selectionBytes() noexcept
    {
        // selectedStart(4) + selectedEnd(4) = 8 bytes
        return 2 * sizeof(std::int32_t);
    }

    float sanitizeUnitRange(float v) noexcept
    {
        if (!std::isfinite(v))
            return 0.0f;
        return juce::jlimit(0.0f, 1.0f, v);
    }

    float sanitizeFinite(float v, float fallback) noexcept
    {
        return std::isfinite(v) ? v : fallback;
    }
} // anonymous namespace

void Sequence::serialize(juce::MemoryBlock& mb) const
{
    const auto hdr = headerBytes();
    const auto stepBytes = static_cast<size_t>(numSteps_) * stepFieldBytes;
    const auto sel = selectionBytes();
    const auto totalBytes = hdr + stepBytes + sel;

    mb.ensureSize(static_cast<size_t>(totalBytes));

    auto* data = static_cast<std::uint8_t*>(mb.getData());
    size_t offset = 0;

    // Header: magic(4) + version(2) + numSteps(4) + timingMode(1)
    writeU32(data + offset, magicNumber);
    offset += sizeof(std::uint32_t);

    writeU16(data + offset, currentVersion);
    offset += sizeof(std::uint16_t);

    writeI32(data + offset, numSteps_);
    offset += sizeof(std::int32_t);

    writeU8(data + offset, static_cast<std::uint8_t>(timingMode_));
    offset += sizeof(std::uint8_t);

    // Steps: explicit fixed-width fields, no struct padding
    for (int i = 0; i < numSteps_; ++i) {
        const auto& step = steps_[i];

        // gate: uint8
        writeU8(data + offset, step.gate ? 1 : 0);
        offset += 1;

        // noteOffset: int32
        writeI32(data + offset, step.noteOffset);
        offset += 4;

        // velocity: float32
        writeFloat32(data + offset, step.velocity);
        offset += 4;

        // accent: uint8
        writeU8(data + offset, step.accent ? 1 : 0);
        offset += 1;

        // probability: float32
        writeFloat32(data + offset, step.probability);
        offset += 4;

        // ratchetCount: int32
        writeI32(data + offset, step.ratchetCount);
        offset += 4;

        // slideDuration: float32
        writeFloat32(data + offset, step.slideDuration);
        offset += 4;

        // gateWidth: float32
        writeFloat32(data + offset, step.gateWidth);
        offset += 4;
    }

    // Selection state: selectedStart(4) + selectedEnd(4)
    writeI32(data + offset, selectedStart);
    offset += 4;
    writeI32(data + offset, selectedEnd);
    offset += 4;
}

bool Sequence::isValidSerialization(const juce::MemoryBlock& mb)
{
    const auto hdr = headerBytes();
    if (mb.getSize() < hdr)
        return false;

    const auto* data = static_cast<const std::uint8_t*>(mb.getData());

    // Magic
    if (readU32(data) != magicNumber)
        return false;

    // Version: only v2 is accepted. v1 (raw struct layout) is intentionally
    // not supported because it depended on compiler ABI/padding/bool repr.
    const auto version = readU16(data + sizeof(std::uint32_t));
    if (version != currentVersion)
        return false;

    // numSteps must be in [1, maxSteps]
    const auto numSteps = readI32(data + sizeof(std::uint32_t) + sizeof(std::uint16_t));
    if (numSteps < 1 || numSteps > maxSteps)
        return false;

    // Timing mode must be valid
    const auto tm = readU8(data + sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::int32_t));
    if (!isValidTimingMode(tm))
        return false;

    // The full payload must be present; partial data is corrupt.
    const auto expectedBytes = hdr + static_cast<size_t>(numSteps) * stepFieldBytes + selectionBytes();
    return mb.getSize() >= expectedBytes;
}

Sequence Sequence::deserialize(const juce::MemoryBlock& mb)
{
    if (!isValidSerialization(mb))
        return {};

    // Version dispatch. v1 (raw struct layout) is intentionally not supported
    // because it depended on compiler ABI/padding/bool representation.
    return loadV2(mb);
}

Sequence Sequence::loadV2(const juce::MemoryBlock& mb)
{
    Sequence seq;
    const auto* data = static_cast<const std::uint8_t*>(mb.getData());
    size_t offset = headerBytes(); // skip header (already validated by caller)

    // Read numSteps from local copy (already validated in isValidSerialization)
    const auto numSteps = readI32(data + sizeof(std::uint32_t) + sizeof(std::uint16_t));
    seq.numSteps_ = juce::jlimit(1, maxSteps, numSteps);

    // Read timing mode
    const auto tm = readU8(data + sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::int32_t));
    seq.timingMode_ = static_cast<TimingMode>(tm);

    // Steps: explicit fixed-width fields, no struct padding
    for (int i = 0; i < seq.numSteps_; ++i) {
        Step step;

        // gate: uint8
        step.gate = readU8(data + offset) != 0;
        offset += 1;

        // noteOffset: int32
        step.noteOffset = juce::jlimit(-24, 24, readI32(data + offset));
        offset += 4;

        // velocity: float32
        step.velocity = sanitizeUnitRange(readFloat32(data + offset));
        offset += 4;

        // accent: uint8
        step.accent = readU8(data + offset) != 0;
        offset += 1;

        // probability: float32
        step.probability = sanitizeUnitRange(readFloat32(data + offset));
        offset += 4;

        // ratchetCount: int32
        step.ratchetCount = juce::jlimit(1, 8, readI32(data + offset));
        offset += 4;

        // slideDuration: float32
        step.slideDuration = sanitizeFinite(readFloat32(data + offset), 0.0f);
        step.slideDuration = juce::jlimit(0.0f, 8.0f, step.slideDuration);
        offset += 4;

        // gateWidth: float32
        step.gateWidth = sanitizeUnitRange(readFloat32(data + offset));
        offset += 4;

        seq.steps_[i] = step;
    }

    // Selection state. The canonical "no selection" sentinel is
    // selectedStart=0, selectedEnd=-1. We must preserve this exactly;
    // clamping -1 would turn an empty selection into "step 0 selected".
    // Accept either the no-selection sentinel or a valid range
    // (0 <= start <= end < numSteps). Anything else is malformed.
    const auto rawStart = readI32(data + offset);
    offset += 4;
    const auto rawEnd = readI32(data + offset);

    const bool noSelection =
        (rawStart == 0 && rawEnd == -1);
    const bool validSelection =
        (rawStart >= 0 && rawStart < numSteps
         && rawEnd >= rawStart && rawEnd < numSteps);

    if (noSelection) {
        seq.selectedStart = 0;
        seq.selectedEnd = -1;
    } else if (validSelection) {
        seq.selectedStart = rawStart;
        seq.selectedEnd = rawEnd;
    } else {
        // Malformed selection: default to no selection
        seq.selectedStart = 0;
        seq.selectedEnd = -1;
    }

    return seq;
}

void Sequence::clear()
{
    for (int i = 0; i < numSteps_; ++i) {
        steps_[i] = Step {};
    }
}

void Sequence::copyFrom(const Sequence& other)
{
    numSteps_ = other.numSteps_;
    timingMode_ = other.timingMode_;
    for (int i = 0; i < maxSteps; ++i) {
        steps_[i] = other.steps_[i];
    }
}

Sequence Sequence::copy() const
{
    Sequence dst;
    dst.numSteps_ = numSteps_;
    dst.timingMode_ = timingMode_;
    for (int i = 0; i < numSteps_; ++i)
        dst.steps_[i] = steps_[i];
    return dst;
}

void Sequence::paste(const Sequence& from)
{
    numSteps_ = from.numSteps_;
    timingMode_ = from.timingMode_;
    for (int i = 0; i < numSteps_; ++i) {
        steps_[i] = from.steps_[i];
    }
}

void Sequence::rotateLeft(int steps)
{
    if (numSteps_ <= 1)
        return;
    steps = juce::jlimit(1, numSteps_ - 1, steps);
    for (int s = 0; s < steps; ++s) {
        const Step first = steps_[0];
        for (int i = 0; i < numSteps_ - 1; ++i) {
            steps_[i] = steps_[i + 1];
        }
        steps_[numSteps_ - 1] = first;
    }
}

void Sequence::rotateRight(int steps)
{
    if (numSteps_ <= 1)
        return;
    steps = juce::jlimit(1, numSteps_ - 1, steps);
    for (int s = 0; s < steps; ++s) {
        const Step last = steps_[numSteps_ - 1];
        for (int i = numSteps_ - 1; i > 0; --i) {
            steps_[i] = steps_[i - 1];
        }
        steps_[0] = last;
    }
}

void Sequence::reverse()
{
    for (int i = 0; i < numSteps_ / 2; ++i) {
        std::swap(steps_[i], steps_[numSteps_ - 1 - i]);
    }
}

void Sequence::transpose(int semitones)
{
    for (int i = 0; i < numSteps_; ++i) {
        steps_[i].noteOffset += semitones;
    }
}

void Sequence::octaveUp()
{
    transpose(12);
}

void Sequence::octaveDown()
{
    transpose(-12);
}

void Sequence::shiftLeft(int steps)
{
    if (numSteps_ <= 1)
        return;
    steps = juce::jlimit(1, numSteps_ - 1, steps);
    for (int s = 0; s < steps; ++s) {
        for (int i = 0; i < numSteps_ - 1; ++i) {
            steps_[i] = steps_[i + 1];
        }
        steps_[numSteps_ - 1] = Step {};
    }
}

void Sequence::shiftRight(int steps)
{
    if (numSteps_ <= 1)
        return;
    steps = juce::jlimit(1, numSteps_ - 1, steps);
    for (int s = 0; s < steps; ++s) {
        for (int i = numSteps_ - 1; i > 0; --i) {
            steps_[i] = steps_[i - 1];
        }
        steps_[0] = Step {};
    }
}

void Sequence::mutateSelected(int seed)
{
    if (numSteps_ == 0)
        return;

    const int start = hasSelection() ? selectedStart : 0;
    const int end = hasSelection() ? selectedEnd : numSteps_ - 1;

    // Deterministic per-step mutation from a single seed so the same seed
    // always reproduces the same result.
    std::mt19937 rng(static_cast<unsigned>(seed));
    for (int i = start; i <= end && i < numSteps_; ++i) {
        if (rng() % 3 == 0)
            steps_[i].noteOffset = static_cast<int>(rng() % 13) - 6;
        if (rng() % 4 == 0)
            steps_[i].gate = !steps_[i].gate;
        if (rng() % 5 == 0)
            steps_[i].accent = !steps_[i].accent;
    }
}

void Sequence::clearSelected()
{
    const int start = hasSelection() ? selectedStart : 0;
    const int end = hasSelection() ? selectedEnd : numSteps_ - 1;
    for (int i = start; i <= end && i < numSteps_; ++i) {
        steps_[i] = Step {};
    }
}

void Sequence::regenerateBySeed(std::uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    for (int i = 0; i < numSteps_; ++i) {
        auto& step = steps_[i];
        step.gate = (i % 4) != 0;
        step.accent = (i % 4) == 1;
        step.noteOffset = 0;
        step.velocity = step.accent ? 0.95f : 0.72f;
        step.probability = 1.0f;
        step.ratchetCount = 1;
        step.slideDuration = 0.0f;
        step.gateWidth = 0.75f;

        if (step.gate && chance(rng) < 0.12f)
            step.noteOffset = 12;
    }
}

void Sequence::copyTo(Sequence& dest) const
{
    dest.setNumSteps(numSteps_);
    for (int i = 0; i < numSteps_; ++i) {
        dest.steps_[i] = steps_[i];
    }
}

} // namespace vstengine::generator

