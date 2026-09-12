#include "semanticfx/SemanticFxEngine.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace vstengine::semanticfx {
namespace {
constexpr std::uint32_t stateMagic = 0x31584653u; // "SFX1", little-endian.
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

void writeU32(std::byte* destination, std::uint32_t value) noexcept
{
    destination[0] = static_cast<std::byte>(value & 0xffu);
    destination[1] = static_cast<std::byte>((value >> 8) & 0xffu);
    destination[2] = static_cast<std::byte>((value >> 16) & 0xffu);
    destination[3] = static_cast<std::byte>((value >> 24) & 0xffu);
}

std::uint32_t readU32(const std::byte* source) noexcept
{
    return std::to_integer<std::uint32_t>(source[0])
        | (std::to_integer<std::uint32_t>(source[1]) << 8)
        | (std::to_integer<std::uint32_t>(source[2]) << 16)
        | (std::to_integer<std::uint32_t>(source[3]) << 24);
}

float clampFinite(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -4.0f, 4.0f) : 0.0f;
}

double wrap(double phase) noexcept
{
    return phase - std::floor(phase);
}
} // namespace

bool SemanticFxEngine::prepare(double sampleRate,
                               std::uint32_t maximumBlockSize,
                               std::uint32_t outputChannels) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0
        || sampleRate > 384000.0 || maximumBlockSize == 0
        || outputChannels == 0 || outputChannels > 2)
        return false;
    sampleRate_ = sampleRate;
    maximumBlockSize_ = maximumBlockSize;
    outputChannels_ = outputChannels;
    prepared_ = true;
    reset();
    return true;
}

void SemanticFxEngine::reset() noexcept
{
    voices_.fill({});
    smoothed_ = targets_;
    triggerOrdinal_ = 0;
}

bool SemanticFxEngine::setParameter(std::string_view id, float value) noexcept
{
    if (!std::isfinite(value)) return false;
    for (std::size_t index = 0; index < parameterIds.size(); ++index)
        if (parameterIds[index] == id) {
            targets_[index] = std::clamp(value, 0.0f, 1.0f);
            return true;
        }
    return false;
}

float SemanticFxEngine::parameter(std::string_view id) const noexcept
{
    for (std::size_t index = 0; index < parameterIds.size(); ++index)
        if (parameterIds[index] == id) return targets_[index];
    return 0.0f;
}

std::size_t SemanticFxEngine::activeVoiceCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        voices_.begin(), voices_.end(), [](const Voice& voice) {
            return voice.active;
        }));
}

void SemanticFxEngine::trigger(std::uint8_t note,
                               std::uint8_t velocity) noexcept
{
    auto* selected = &voices_[0];
    for (auto& voice : voices_)
        if (!voice.active) {
            selected = &voice;
            break;
        } else if (static_cast<std::uint64_t>(voice.age) * selected->length
                   > static_cast<std::uint64_t>(selected->age) * voice.length) {
            selected = &voice; // Deterministically steal oldest normalized age.
        }

    const auto familyIndex = static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::lround(targets_[family] * 9.0f)), 0, 9));
    float seconds = 0.08f * std::pow(56.25f, targets_[duration]);
    if (familyIndex == static_cast<std::uint8_t>(EventFamily::impact))
        seconds *= 0.48f;
    else if (familyIndex == static_cast<std::uint8_t>(EventFamily::laser)
             || familyIndex == static_cast<std::uint8_t>(EventFamily::zap))
        seconds *= 0.30f;
    else if (familyIndex == static_cast<std::uint8_t>(EventFamily::riser))
        seconds *= 1.25f;
    seconds = std::clamp(seconds, 0.035f, 4.5f);

    *selected = {};
    selected->event = static_cast<EventFamily>(familyIndex);
    selected->length = std::max<std::uint32_t>(1,
        static_cast<std::uint32_t>(sampleRate_ * seconds));
    selected->randomState = 0x6d2b79f5u
        ^ (static_cast<std::uint32_t>(note) * 0x9e3779b9u)
        ^ (static_cast<std::uint32_t>(velocity) * 0x85ebca6bu)
        ^ (++triggerOrdinal_ * 0xc2b2ae35u);
    if (selected->randomState == 0) selected->randomState = 1;
    selected->velocity = 0.35f
        + 0.65f * (static_cast<float>(velocity) / 127.0f);
    selected->noteRatio = std::exp2(
        (static_cast<float>(note) - 60.0f) / 24.0f);
    selected->active = true;
}

void SemanticFxEngine::allNotesOff() noexcept
{
    for (auto& voice : voices_) voice.active = false;
}

float SemanticFxEngine::noise(Voice& voice) noexcept
{
    auto value = voice.randomState;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    voice.randomState = value;
    return static_cast<float>(value >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

float SemanticFxEngine::smoothEnvelope(float phase) noexcept
{
    const float edge = std::min(1.0f, phase * 24.0f)
        * std::min(1.0f, (1.0f - phase) * 18.0f);
    return std::max(0.0f, edge);
}

float SemanticFxEngine::renderVoice(Voice& voice) noexcept
{
    if (!voice.active || voice.age >= voice.length) {
        voice.active = false;
        return 0.0f;
    }
    const float t = static_cast<float>(voice.age)
        / static_cast<float>(voice.length);
    const float toneValue = 35.0f * std::pow(180.0f, smoothed_[tone])
        * voice.noteRatio;
    const float bright = smoothed_[brightness];
    const float move = smoothed_[motion];
    const float textureAmount = smoothed_[texture];
    const float white = noise(voice);
    float value = 0.0f;
    float frequency = toneValue;
    float envelope = smoothEnvelope(t);

    switch (voice.event) {
        case EventFamily::sweep:
            frequency *= std::exp2((-2.0f + 7.0f * t) * (0.35f + move));
            envelope *= std::sin(pi * std::clamp(t, 0.0f, 1.0f));
            value = std::sin(twoPi * static_cast<float>(voice.phaseA))
                + white * textureAmount * 0.45f;
            break;
        case EventFamily::laser:
            frequency *= 1.0f + (18.0f + 32.0f * move)
                * std::exp(-t * (7.0f + 9.0f * bright));
            envelope = std::exp(-t * (5.0f + 5.0f * textureAmount));
            value = std::sin(twoPi * static_cast<float>(voice.phaseA))
                + 0.3f * std::sin(twoPi * static_cast<float>(voice.phaseA * 2.01));
            break;
        case EventFamily::riser:
            frequency *= std::exp2((-3.0f + 8.0f * t) * (0.45f + move));
            envelope = std::pow(t, 0.45f) * std::min(1.0f, (1.0f - t) * 24.0f);
            value = static_cast<float>(2.0 * voice.phaseA - 1.0)
                + white * (0.25f + 0.55f * textureAmount);
            break;
        case EventFamily::downlifter:
            frequency *= std::exp2((3.5f - 7.0f * t) * (0.4f + move));
            envelope = std::pow(1.0f - t, 0.7f) * std::min(1.0f, t * 30.0f);
            value = white * (0.7f + textureAmount * 0.5f)
                + std::sin(twoPi * static_cast<float>(voice.phaseA)) * 0.3f;
            break;
        case EventFamily::impact:
            frequency *= 0.35f + 7.5f * std::exp(-t * (18.0f + 12.0f * move));
            envelope = std::exp(-t * (5.5f + 4.0f * textureAmount));
            value = std::sin(twoPi * static_cast<float>(voice.phaseA))
                + white * std::exp(-t * 45.0f) * (0.4f + bright);
            break;
        case EventFamily::whoosh:
            frequency *= std::exp2(std::sin(t * pi) * (2.0f + 5.0f * move));
            envelope *= std::sin(pi * t);
            value = white;
            break;
        case EventFamily::zap:
            frequency *= 2.0f + (28.0f + 24.0f * move)
                * std::pow(1.0f - t, 3.0f);
            envelope = std::exp(-t * (9.0f + 8.0f * textureAmount));
            value = (voice.phaseA < 0.5 ? 1.0f : -1.0f) * 0.45f
                + std::sin(twoPi * static_cast<float>(voice.phaseA)) * 0.75f;
            break;
        case EventFamily::noiseBurst:
            frequency *= 0.5f + 7.0f * bright;
            envelope = std::exp(-t * (3.0f + 10.0f * move));
            value = white;
            break;
        case EventFamily::metallic: {
            frequency *= 1.5f + 4.0f * bright;
            envelope = std::exp(-t * (2.2f + 5.0f * textureAmount));
            const float modulator = std::sin(twoPi
                * static_cast<float>(voice.phaseB));
            value = std::sin(twoPi * static_cast<float>(voice.phaseA)
                             + modulator * (2.0f + 9.0f * move))
                * std::sin(twoPi * static_cast<float>(voice.phaseB * 1.4142));
            break;
        }
        case EventFamily::alien: {
            frequency *= 0.45f + 2.2f * (0.5f + 0.5f
                * std::sin(twoPi * (1.0f + 5.0f * move) * t));
            envelope *= 0.65f + 0.35f * std::sin(pi * t);
            const float modulator = std::sin(twoPi
                * static_cast<float>(voice.phaseB));
            value = std::sin(twoPi * static_cast<float>(voice.phaseA)
                             + modulator * (3.0f + 13.0f * textureAmount))
                + white * 0.18f * bright;
            break;
        }
        case EventFamily::count:
            voice.active = false;
            return 0.0f;
    }

    frequency = std::clamp(frequency, 8.0f,
                           static_cast<float>(sampleRate_ * 0.42));
    voice.phaseA = wrap(voice.phaseA + frequency / sampleRate_);
    voice.phaseB = wrap(voice.phaseB + frequency
        * (1.37 + 2.1 * move) / sampleRate_);

    const float cutoff = std::clamp(frequency * (0.7f + 7.0f * bright),
                                    25.0f,
                                    static_cast<float>(sampleRate_ * 0.42));
    const float coefficient = std::clamp(1.0f
        - std::exp(-twoPi * cutoff / static_cast<float>(sampleRate_)),
        0.0f, 0.95f);
    voice.previousLowpass = voice.lowpass;
    voice.lowpass += coefficient * (value - voice.lowpass);
    voice.lowpass = clampFinite(voice.lowpass);
    const float highpass = value - voice.lowpass;
    if (voice.event == EventFamily::whoosh
        || voice.event == EventFamily::noiseBurst
        || voice.event == EventFamily::downlifter)
        value = voice.lowpass * (0.75f + textureAmount * 0.4f)
            + highpass * bright * 0.55f;
    else
        value = voice.lowpass + highpass * bright * 0.3f;

    ++voice.age;
    if (voice.age >= voice.length) voice.active = false;
    return clampFinite(value * envelope * voice.velocity);
}

void SemanticFxEngine::process(std::span<float*> outputs,
                               std::uint32_t sampleCount,
                               std::span<const VoxMidiEventV1> midi) noexcept
{
    if (!prepared_ || sampleCount > maximumBlockSize_ || outputs.empty()
        || outputs.size() > outputChannels_)
        return;
    std::size_t eventIndex = 0;
    const float smoothing = 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate_ * 0.008));
    for (std::uint32_t sample = 0; sample < sampleCount; ++sample) {
        while (eventIndex < midi.size()
               && midi[eventIndex].sampleOffset <= sample) {
            const auto& event = midi[eventIndex++];
            if (event.sampleOffset != sample || event.size == 0) continue;
            const auto type = event.data[0] & 0xf0u;
            if (type == 0x90u && event.size >= 3 && event.data[2] != 0)
                trigger(event.data[1], event.data[2]);
            else if (type == 0xb0u && event.size >= 3
                     && (event.data[1] == 120 || event.data[1] == 123))
                allNotesOff();
            // NoteOff is intentionally ignored: every semantic event is a
            // bounded one-shot and ordinary MIDI note length cannot cut it.
        }
        for (std::size_t index = 1; index < smoothed_.size(); ++index)
            smoothed_[index] += smoothing * (targets_[index] - smoothed_[index]);
        smoothed_[family] = targets_[family];

        float mixed = 0.0f;
        for (auto& voice : voices_) mixed += renderVoice(voice);
        mixed = std::tanh(mixed * (1.0f + 7.0f * smoothed_[drive]
                                   * smoothed_[drive]));
        mixed *= (0.04f + 0.34f * smoothed_[output]);
        if (!std::isfinite(mixed)) {
            allNotesOff();
            mixed = 0.0f;
        }
        for (auto* channel : outputs)
            if (channel != nullptr) channel[sample] += mixed;
    }
}

bool SemanticFxEngine::saveState(std::span<std::byte> destination,
                                 std::uint32_t& bytesWritten) const noexcept
{
    bytesWritten = static_cast<std::uint32_t>(encodedStateBytes);
    if (destination.size() < encodedStateBytes) return false;
    writeU32(destination.data(), stateMagic);
    writeU32(destination.data() + 4, stateVersion);
    for (std::size_t index = 0; index < targets_.size(); ++index)
        writeU32(destination.data() + 8 + index * 4,
                 std::bit_cast<std::uint32_t>(targets_[index]));
    return true;
}

bool SemanticFxEngine::loadState(std::uint32_t schemaVersion,
                                 std::span<const std::byte> payload) noexcept
{
    if (schemaVersion != stateVersion) return false;
    if (payload.empty()) return true;
    if (payload.size() != encodedStateBytes
        || readU32(payload.data()) != stateMagic
        || readU32(payload.data() + 4) != stateVersion)
        return false;
    std::array<float, parameterCount> decoded {};
    for (std::size_t index = 0; index < decoded.size(); ++index) {
        decoded[index] = std::bit_cast<float>(
            readU32(payload.data() + 8 + index * 4));
        if (!std::isfinite(decoded[index]) || decoded[index] < 0.0f
            || decoded[index] > 1.0f)
            return false;
    }
    targets_ = decoded;
    smoothed_ = decoded;
    return true;
}

std::uint32_t SemanticFxEngine::tailSamples() const noexcept
{
    return static_cast<std::uint32_t>(sampleRate_ * 4.5);
}

} // namespace vstengine::semanticfx
