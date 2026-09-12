#include "atmos/AtmosEngine.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace vstengine::atmos {
namespace {
constexpr std::uint32_t stateMagic = 0x4f4d5441u; // "ATMO", little-endian.
constexpr float pi = 3.14159265358979323846f;

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

float coefficient(double sampleRate, float seconds) noexcept
{
    return 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate * std::max(seconds, 0.0001f)));
}
} // namespace

bool AtmosEngine::prepare(double sampleRate, std::uint32_t maximumBlockSize,
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

void AtmosEngine::reset() noexcept
{
    voices_ = {};
    voiceAge_ = 0;
    smoothed_ = targets_;
    for (std::size_t index = 0; index < voices_.size(); ++index)
        voices_[index].randomState = 0x6d2b79f5u
            ^ static_cast<std::uint32_t>((index + 1) * 0x9e3779b9u);
}

bool AtmosEngine::setParameter(std::string_view id, float value) noexcept
{
    if (!std::isfinite(value)) return false;
    for (std::size_t index = 0; index < parameterIds.size(); ++index)
        if (parameterIds[index] == id) {
            targets_[index] = std::clamp(value, 0.0f, 1.0f);
            return true;
        }
    return false;
}

float AtmosEngine::parameter(std::string_view id) const noexcept
{
    for (std::size_t index = 0; index < parameterIds.size(); ++index)
        if (parameterIds[index] == id) return targets_[index];
    return 0.0f;
}

double AtmosEngine::midiFrequency(float note) noexcept
{
    return 440.0 * std::exp2((static_cast<double>(note) - 69.0) / 12.0);
}

float AtmosEngine::polyBlep(double phase, double increment) noexcept
{
    increment = std::clamp(increment, 1.0e-9, 0.49);
    if (phase < increment) {
        const auto x = phase / increment;
        return static_cast<float>(x + x - x * x - 1.0);
    }
    if (phase > 1.0 - increment) {
        const auto x = (phase - 1.0) / increment;
        return static_cast<float>(x * x + x + x + 1.0);
    }
    return 0.0f;
}

float AtmosEngine::renderOscillator(double& phase, double increment,
                                    float shape) noexcept
{
    const float sine = std::sin(static_cast<float>(2.0 * pi * phase));
    const float saw = static_cast<float>(2.0 * phase - 1.0)
        - polyBlep(phase, increment);
    const float triangle = 1.0f - 4.0f
        * std::abs(static_cast<float>(phase) - 0.5f);
    const float bright = saw + (triangle - saw) * shape;
    const float result = sine + (bright - sine) * (0.2f + 0.65f * shape);
    phase += increment;
    phase -= std::floor(phase);
    return result;
}

float AtmosEngine::noise(Voice& voice) noexcept
{
    voice.randomState ^= voice.randomState << 13;
    voice.randomState ^= voice.randomState >> 17;
    voice.randomState ^= voice.randomState << 5;
    return static_cast<float>(voice.randomState >> 8)
        * (2.0f / 16777216.0f) - 1.0f;
}

void AtmosEngine::noteOn(std::uint8_t note, std::uint8_t velocity) noexcept
{
    Voice* selected = nullptr;
    for (auto& voice : voices_)
        if (!voice.active) { selected = &voice; break; }
    if (selected == nullptr)
        selected = &*std::min_element(voices_.begin(), voices_.end(),
            [](const Voice& left, const Voice& right) {
                return left.age < right.age;
            });

    *selected = {};
    selected->active = true;
    selected->note = note;
    selected->velocity = 0.28f
        + 0.72f * static_cast<float>(velocity) / 127.0f;
    selected->age = ++voiceAge_;
    selected->stage = EnvelopeStage::attack;
    selected->randomState = 0xa511e9b3u
        ^ (static_cast<std::uint32_t>(note) * 0x9e3779b9u)
        ^ static_cast<std::uint32_t>(selected->age);
    selected->pan = static_cast<float>((selected->randomState >> 8) & 0xffffu)
        * (2.0f / 65535.0f) - 1.0f;
    selected->slowPhase = static_cast<double>(selected->randomState & 0xffffu)
        / 65536.0;
    selected->evolutionPhase = static_cast<double>(
        (selected->randomState >> 16) & 0xffffu) / 65536.0;
    for (std::size_t oscillator = 0; oscillator < oscillatorCount; ++oscillator) {
        const double initial = static_cast<double>((selected->randomState
            + static_cast<std::uint32_t>(oscillator) * 0x6d2b79f5u) & 0xffffu)
            / 65536.0;
        selected->phase[oscillator] = initial;
        selected->phaseRight[oscillator] = std::fmod(initial + 0.19
            + 0.11 * static_cast<double>(oscillator), 1.0);
    }
}

void AtmosEngine::noteOff(std::uint8_t note) noexcept
{
    for (auto& voice : voices_)
        if (voice.active && voice.note == note)
            voice.stage = EnvelopeStage::release;
}

void AtmosEngine::allNotesOff() noexcept
{
    for (auto& voice : voices_)
        if (voice.active) voice.stage = EnvelopeStage::release;
}

void AtmosEngine::updateEnvelope(Voice& voice) noexcept
{
    const float attackSeconds = 0.02f * std::pow(250.0f, smoothed_[attack]);
    const float decaySeconds = 0.35f * std::pow(18.0f, smoothed_[decay]);
    // Contract-wide rack budget caps bounded tails at five seconds at 384 kHz.
    // Half-second maximum time constant reaches -70 dB inside reported tail.
    const float releaseSeconds = 0.075f
        * std::pow(6.6666665f, smoothed_[release]);
    const float sustainLevel = 0.16f + 0.82f * smoothed_[sustain];
    switch (voice.stage) {
        case EnvelopeStage::attack:
            voice.envelope += coefficient(sampleRate_, attackSeconds)
                * (1.02f - voice.envelope);
            voice.filterEnvelope += coefficient(sampleRate_, attackSeconds * 0.7f)
                * (1.0f - voice.filterEnvelope);
            if (voice.envelope >= 1.0f) voice.stage = EnvelopeStage::decay;
            break;
        case EnvelopeStage::decay:
            voice.envelope += coefficient(sampleRate_, decaySeconds)
                * (sustainLevel - voice.envelope);
            voice.filterEnvelope += coefficient(sampleRate_, decaySeconds)
                * (0.25f - voice.filterEnvelope);
            if (std::abs(voice.envelope - sustainLevel) < 0.001f)
                voice.stage = EnvelopeStage::sustain;
            break;
        case EnvelopeStage::sustain:
            voice.envelope = sustainLevel;
            break;
        case EnvelopeStage::release:
            voice.envelope += coefficient(sampleRate_, releaseSeconds)
                * -voice.envelope;
            voice.filterEnvelope += coefficient(sampleRate_, releaseSeconds * 0.7f)
                * -voice.filterEnvelope;
            if (voice.envelope < 3.0e-4f) {
                const auto seed = voice.randomState;
                voice = {};
                voice.randomState = seed == 0 ? 1u : seed;
            }
            break;
        case EnvelopeStage::off: break;
    }
}

float AtmosEngine::filtered(Voice& voice, float input, float cutoffHz,
                            bool right) noexcept
{
    auto& low = right ? voice.lowRight : voice.lowLeft;
    auto& band = right ? voice.bandRight : voice.bandLeft;
    const float frequency = std::clamp(2.0f * std::sin(pi * cutoffHz
        / static_cast<float>(sampleRate_)), 0.0001f, 0.92f);
    const float damping = 1.95f - 1.72f * smoothed_[resonance];
    low += frequency * band;
    const float high = input - low - damping * band;
    band += frequency * high;
    low = std::clamp(low, -6.0f, 6.0f);
    band = std::clamp(band, -6.0f, 6.0f);
    return low + band * (0.08f + 0.42f * smoothed_[texture]);
}

void AtmosEngine::process(std::span<float*> outputs, std::uint32_t sampleCount,
                          std::span<const VoxMidiEventV1> midi) noexcept
{
    if (!prepared_ || sampleCount > maximumBlockSize_ || outputs.empty()
        || outputs.size() > outputChannels_)
        return;
    std::size_t eventIndex = 0;
    const float smoothing = coefficient(sampleRate_, 0.03f);
    for (std::uint32_t sample = 0; sample < sampleCount; ++sample) {
        while (eventIndex < midi.size() && midi[eventIndex].sampleOffset <= sample) {
            const auto& event = midi[eventIndex++];
            if (event.sampleOffset != sample || event.size == 0) continue;
            const auto type = event.data[0] & 0xf0u;
            if (type == 0x90u && event.size >= 3 && event.data[2] != 0)
                noteOn(event.data[1], event.data[2]);
            else if ((type == 0x80u || type == 0x90u) && event.size >= 3)
                noteOff(event.data[1]);
            else if (type == 0xb0u && event.size >= 3
                     && (event.data[1] == 120 || event.data[1] == 123))
                allNotesOff();
        }
        for (std::size_t index = 0; index < smoothed_.size(); ++index)
            smoothed_[index] += smoothing * (targets_[index] - smoothed_[index]);

        float left = 0.0f;
        float right = 0.0f;
        for (auto& voice : voices_) {
            if (!voice.active) continue;
            updateEnvelope(voice);
            if (!voice.active) continue;
            const float slow = std::sin(static_cast<float>(2.0 * pi
                * voice.slowPhase));
            const float evolve = std::sin(static_cast<float>(2.0 * pi
                * voice.evolutionPhase));
            const float driftCents = (slow * 8.0f + evolve * 5.0f)
                * smoothed_[drift];
            const double baseFrequency = std::min(sampleRate_ * 0.12,
                midiFrequency(static_cast<float>(voice.note) + driftCents / 100.0f));
            const std::size_t partialCount = static_cast<std::size_t>(1
                + std::lround(smoothed_[density] * (oscillatorCount - 1)));
            float oscillatorLeft = 0.0f;
            float oscillatorRight = 0.0f;
            for (std::size_t oscillator = 0; oscillator < partialCount; ++oscillator) {
                const float position = oscillatorCount == 1 ? 0.0f
                    : 2.0f * static_cast<float>(oscillator)
                        / static_cast<float>(oscillatorCount - 1) - 1.0f;
                const float ratio = oscillator == 0 ? 1.0f
                    : 1.0f + static_cast<float>(oscillator)
                        * (0.495f + 0.19f * smoothed_[harmonics]);
                const float cents = position * (4.0f + 23.0f * smoothed_[spread]);
                const double incrementLeft = std::min(0.42,
                    baseFrequency * ratio * std::exp2(cents / 1200.0f) / sampleRate_);
                const double incrementRight = std::min(0.42,
                    baseFrequency * ratio * std::exp2(-cents / 1200.0f) / sampleRate_);
                const float level = 1.0f / (1.0f + static_cast<float>(oscillator));
                const float shape = std::clamp(smoothed_[blend]
                    + evolve * 0.18f * smoothed_[evolution], 0.0f, 1.0f);
                oscillatorLeft += level * renderOscillator(
                    voice.phase[oscillator], incrementLeft, shape);
                oscillatorRight += level * renderOscillator(
                    voice.phaseRight[oscillator], incrementRight, shape);
            }
            const float random = noise(voice);
            const float textureAmount = smoothed_[texture]
                * (0.05f + 0.32f * smoothed_[blend]);
            oscillatorLeft += random * textureAmount;
            oscillatorRight -= random * textureAmount * 0.83f;
            const float baseCutoff = 90.0f * std::pow(150.0f,
                smoothed_[brightness]);
            const float animated = std::exp2((slow * smoothed_[motion]
                + evolve * smoothed_[evolution]) * 1.6f
                + voice.filterEnvelope * 0.8f);
            const float cutoffHz = std::clamp(baseCutoff * animated, 45.0f,
                static_cast<float>(sampleRate_ * 0.40));
            float voiceLeft = filtered(voice, oscillatorLeft, cutoffHz, false);
            float voiceRight = filtered(voice, oscillatorRight,
                cutoffHz * (1.0f + 0.025f * smoothed_[spread]), true);
            const float diffusion = 0.12f + 0.68f * smoothed_[space];
            const float previousLeft = voice.diffusionLeft;
            voice.diffusionLeft = voiceLeft + diffusion * voice.diffusionRight;
            voice.diffusionRight = voiceRight - diffusion * previousLeft;
            voiceLeft = voiceLeft * (1.0f - 0.35f * diffusion)
                + voice.diffusionLeft * 0.35f;
            voiceRight = voiceRight * (1.0f - 0.35f * diffusion)
                + voice.diffusionRight * 0.35f;
            const float pan = voice.pan * smoothed_[spread];
            left += voiceLeft * voice.envelope * voice.velocity * (0.72f - 0.24f * pan);
            right += voiceRight * voice.envelope * voice.velocity * (0.72f + 0.24f * pan);
            voice.slowPhase += (0.012 + 0.19 * smoothed_[motion]) / sampleRate_;
            voice.slowPhase -= std::floor(voice.slowPhase);
            voice.evolutionPhase += (0.003 + 0.047 * smoothed_[evolution])
                / sampleRate_;
            voice.evolutionPhase -= std::floor(voice.evolutionPhase);
            if (!std::isfinite(left) || !std::isfinite(right)
                || !std::isfinite(voice.lowLeft) || !std::isfinite(voice.lowRight)) {
                voice = {};
                left = right = 0.0f;
            }
        }
        const float gain = 0.018f + 0.095f * smoothed_[output];
        left = std::clamp(std::tanh(left * gain), -1.0f, 1.0f);
        right = std::clamp(std::tanh(right * gain), -1.0f, 1.0f);
        if (outputs.size() == 1) {
            if (outputs[0] != nullptr) outputs[0][sample] += 0.5f * (left + right);
        } else {
            if (outputs[0] != nullptr) outputs[0][sample] += left;
            if (outputs[1] != nullptr) outputs[1][sample] += right;
        }
    }
}

bool AtmosEngine::saveState(std::span<std::byte> destination,
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

bool AtmosEngine::loadState(std::uint32_t schemaVersion,
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
    targets_ = smoothed_ = decoded;
    return true;
}

std::uint32_t AtmosEngine::tailSamples() const noexcept
{
    const double releaseSeconds = 0.075
        * std::pow(6.6666665, targets_[release]);
    const double samples = sampleRate_ * releaseSeconds * 8.0;
    return static_cast<std::uint32_t>(std::min(samples,
        static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
}

} // namespace vstengine::atmos
