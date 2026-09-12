#include "lead/LeadEngine.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace vstengine::lead {
namespace {
constexpr std::uint32_t stateMagic = 0x4441454cu; // "LEAD", little-endian.
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

bool LeadEngine::prepare(double sampleRate, std::uint32_t maximumBlockSize,
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

void LeadEngine::reset() noexcept
{
    voices_ = {};
    for (std::size_t index = 0; index < voices_.size(); ++index)
        voices_[index].noiseState = 0x9e3779b9u
            ^ static_cast<std::uint32_t>((index + 1) * 0x85ebca6bu);
    pitchBendSemitones_ = 0.0f;
    voiceAge_ = 0;
    smoothed_ = targets_;
}

bool LeadEngine::setParameter(std::string_view id, float value) noexcept
{
    if (!std::isfinite(value)) return false;
    for (std::size_t index = 0; index < parameterIds.size(); ++index)
        if (parameterIds[index] == id) {
            targets_[index] = std::clamp(value, 0.0f, 1.0f);
            return true;
        }
    return false;
}

float LeadEngine::parameter(std::string_view id) const noexcept
{
    for (std::size_t index = 0; index < parameterIds.size(); ++index)
        if (parameterIds[index] == id) return targets_[index];
    return 0.0f;
}

double LeadEngine::midiFrequency(float note) noexcept
{
    return 440.0 * std::exp2((static_cast<double>(note) - 69.0) / 12.0);
}

float LeadEngine::polyBlep(double phase, double increment) noexcept
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

void LeadEngine::noteOn(std::uint8_t note, std::uint8_t velocity) noexcept
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
    selected->active = selected->gate = true;
    selected->note = note;
    selected->velocity = 0.35f + 0.65f * static_cast<float>(velocity) / 127.0f;
    selected->age = ++voiceAge_;
    selected->stage = EnvelopeStage::attack;
    selected->noiseState = 0xa511e9b3u
        ^ (static_cast<std::uint32_t>(note) * 0x9e3779b9u)
        ^ static_cast<std::uint32_t>(selected->age);
}

void LeadEngine::noteOff(std::uint8_t note) noexcept
{
    for (auto& voice : voices_)
        if (voice.active && voice.note == note) {
            voice.gate = false;
            voice.stage = EnvelopeStage::release;
        }
}

void LeadEngine::allNotesOff() noexcept
{
    for (auto& voice : voices_)
        if (voice.active) {
            voice.gate = false;
            voice.stage = EnvelopeStage::release;
        }
}

void LeadEngine::pitchBend(std::uint8_t lsb, std::uint8_t msb) noexcept
{
    const int value = static_cast<int>(lsb) | (static_cast<int>(msb) << 7);
    pitchBendSemitones_ = (static_cast<float>(value) - 8192.0f)
        * (2.0f / 8192.0f);
}

float LeadEngine::oscillator(Voice& voice, std::size_t oscillatorIndex,
                             double increment, float) noexcept
{
    auto& phase = voice.phase[oscillatorIndex];
    auto& modPhase = voice.modPhase[oscillatorIndex];
    const float fmDepth = 0.22f * smoothed_[fm] * smoothed_[fm];
    double shapedPhase = phase + fmDepth * std::sin(2.0 * pi * modPhase);
    shapedPhase -= std::floor(shapedPhase);

    const double syncRatio = 1.0 + 3.0 * smoothed_[sync];
    double syncPhase = shapedPhase * syncRatio;
    syncPhase -= std::floor(syncPhase);
    const double slaveIncrement = std::min(0.49, increment * syncRatio);
    const float saw = static_cast<float>(2.0 * syncPhase - 1.0)
        - polyBlep(syncPhase, slaveIncrement);
    const double pulseWidth = 0.18 + 0.64 * smoothed_[morph];
    float pulse = syncPhase < pulseWidth ? 1.0f : -1.0f;
    pulse += polyBlep(syncPhase, slaveIncrement);
    auto edge = syncPhase - pulseWidth;
    if (edge < 0.0) edge += 1.0;
    pulse -= polyBlep(edge, slaveIncrement);
    const float triangle = 1.0f - 4.0f
        * std::abs(static_cast<float>(syncPhase) - 0.5f);
    const float morphValue = smoothed_[morph];
    float result = morphValue < 0.5f
        ? saw + (pulse - saw) * (morphValue * 2.0f)
        : pulse + (triangle - pulse) * ((morphValue - 0.5f) * 2.0f);
    const float ringSignal = std::sin(static_cast<float>(4.0 * pi * modPhase));
    result *= 1.0f - smoothed_[ring]
        + smoothed_[ring] * ringSignal;

    phase += increment;
    phase -= std::floor(phase);
    modPhase += std::min(0.49, increment * (1.99 + smoothed_[fm]));
    modPhase -= std::floor(modPhase);
    return result;
}

float LeadEngine::filter(Voice& voice, float input, float cutoffHz,
                         float resonanceAmount) noexcept
{
    const float g = std::tan(pi * std::clamp(cutoffHz
        / static_cast<float>(sampleRate_), 0.00005f, 0.44f));
    const float damping = 2.0f - 1.92f * resonanceAmount;
    const float a1 = 1.0f / (1.0f + g * (g + damping));
    const float v1 = a1 * (voice.filterIntegrator1
        + g * (input - voice.filterIntegrator2));
    const float v2 = voice.filterIntegrator2 + g * v1;
    voice.filterIntegrator1 = std::clamp(2.0f * v1
        - voice.filterIntegrator1, -8.0f, 8.0f);
    voice.filterIntegrator2 = std::clamp(2.0f * v2
        - voice.filterIntegrator2, -8.0f, 8.0f);
    return v2;
}

void LeadEngine::updateEnvelope(Voice& voice) noexcept
{
    const float attackSeconds = 0.001f
        * std::pow(300.0f, smoothed_[attack]);
    const float decaySeconds = 0.025f
        * std::pow(80.0f, smoothed_[decay]);
    const float releaseSeconds = 0.025f
        * std::pow(40.0f, smoothed_[release]);
    const float sustainLevel = 0.08f + 0.9f * smoothed_[sustain];
    switch (voice.stage) {
        case EnvelopeStage::attack:
            voice.ampEnvelope += coefficient(sampleRate_, attackSeconds)
                * (1.04f - voice.ampEnvelope);
            voice.filterEnvelope += coefficient(sampleRate_, attackSeconds * 0.65f)
                * (1.04f - voice.filterEnvelope);
            if (voice.ampEnvelope >= 1.0f) voice.stage = EnvelopeStage::decay;
            break;
        case EnvelopeStage::decay:
            voice.ampEnvelope += coefficient(sampleRate_, decaySeconds)
                * (sustainLevel - voice.ampEnvelope);
            voice.filterEnvelope += coefficient(sampleRate_, decaySeconds * 0.55f)
                * (0.08f - voice.filterEnvelope);
            if (std::abs(voice.ampEnvelope - sustainLevel) < 0.001f)
                voice.stage = EnvelopeStage::sustain;
            break;
        case EnvelopeStage::sustain:
            voice.ampEnvelope = sustainLevel;
            voice.filterEnvelope += coefficient(sampleRate_, 0.08f)
                * (0.08f - voice.filterEnvelope);
            break;
        case EnvelopeStage::release:
            voice.ampEnvelope += coefficient(sampleRate_, releaseSeconds)
                * -voice.ampEnvelope;
            voice.filterEnvelope += coefficient(sampleRate_, releaseSeconds * 0.5f)
                * -voice.filterEnvelope;
            if (voice.ampEnvelope < 1.0e-5f) {
                voice = {};
                voice.noiseState = 1;
            }
            break;
        case EnvelopeStage::off: break;
    }
}

void LeadEngine::process(std::span<float*> outputs, std::uint32_t sampleCount,
                         std::span<const VoxMidiEventV1> midi) noexcept
{
    if (!prepared_ || sampleCount > maximumBlockSize_ || outputs.empty()
        || outputs.size() > outputChannels_)
        return;
    std::size_t eventIndex = 0;
    const float smoothing = coefficient(sampleRate_, 0.008f);
    for (std::uint32_t sample = 0; sample < sampleCount; ++sample) {
        while (eventIndex < midi.size() && midi[eventIndex].sampleOffset <= sample) {
            const auto& event = midi[eventIndex++];
            if (event.sampleOffset != sample || event.size == 0) continue;
            const auto type = event.data[0] & 0xf0u;
            if (type == 0x90u && event.size >= 3 && event.data[2] != 0)
                noteOn(event.data[1], event.data[2]);
            else if ((type == 0x80u || type == 0x90u) && event.size >= 3)
                noteOff(event.data[1]);
            else if (type == 0xe0u && event.size >= 3)
                pitchBend(event.data[1], event.data[2]);
            else if (type == 0xb0u && event.size >= 3
                     && (event.data[1] == 120 || event.data[1] == 123))
                allNotesOff();
        }
        for (std::size_t index = 0; index < smoothed_.size(); ++index)
            smoothed_[index] += smoothing * (targets_[index] - smoothed_[index]);

        float mixed = 0.0f;
        for (auto& voice : voices_) {
            if (!voice.active) continue;
            updateEnvelope(voice);
            if (!voice.active) continue;
            const auto unisonCount = static_cast<std::size_t>(1
                + std::lround(smoothed_[unison] * (maximumUnison - 1)));
            const double baseFrequency = std::min(sampleRate_ * 0.22,
                midiFrequency(static_cast<float>(voice.note) + pitchBendSemitones_));
            float oscillatorMix = 0.0f;
            for (std::size_t oscillatorIndex = 0; oscillatorIndex < unisonCount;
                 ++oscillatorIndex) {
                const float position = unisonCount == 1 ? 0.0f
                    : 2.0f * static_cast<float>(oscillatorIndex)
                        / static_cast<float>(unisonCount - 1) - 1.0f;
                const float cents = 34.0f * smoothed_[detune]
                    * smoothed_[detune] * position;
                const double increment = std::min(0.45,
                    baseFrequency * std::exp2(cents / 1200.0) / sampleRate_);
                oscillatorMix += oscillator(voice, oscillatorIndex, increment,
                                             position);
            }
            oscillatorMix /= std::sqrt(static_cast<float>(unisonCount));
            voice.noiseState ^= voice.noiseState << 13;
            voice.noiseState ^= voice.noiseState >> 17;
            voice.noiseState ^= voice.noiseState << 5;
            const float noiseSample = static_cast<float>(voice.noiseState >> 8)
                * (2.0f / 16777216.0f) - 1.0f;
            oscillatorMix += noiseSample * 0.24f * smoothed_[noise];
            const float driven = std::tanh(oscillatorMix
                * (1.0f + 7.0f * smoothed_[drive] * smoothed_[drive]));
            const float baseCutoff = 45.0f
                * std::pow(420.0f, smoothed_[cutoff]);
            const float cutoffHz = std::clamp(baseCutoff * std::exp2(
                voice.filterEnvelope * (1.0f + 6.0f * smoothed_[filterEnvelope])),
                35.0f, static_cast<float>(sampleRate_ * 0.43));
            float value = filter(voice, driven, cutoffHz, smoothed_[resonance]);
            value = std::tanh(value * (1.0f + 1.8f * smoothed_[drive]));
            mixed += value * voice.ampEnvelope * voice.velocity;
            if (!std::isfinite(mixed) || !std::isfinite(voice.filterIntegrator2)) {
                voice = {};
                mixed = 0.0f;
            }
        }
        mixed *= (0.035f + 0.19f * smoothed_[output]);
        mixed = std::clamp(mixed, -1.5f, 1.5f);
        for (auto* channel : outputs)
            if (channel != nullptr) channel[sample] += mixed;
    }
}

bool LeadEngine::saveState(std::span<std::byte> destination,
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

bool LeadEngine::loadState(std::uint32_t schemaVersion,
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

std::uint32_t LeadEngine::tailSamples() const noexcept
{
    const float releaseSeconds = 0.025f * std::pow(40.0f, targets_[release]);
    return static_cast<std::uint32_t>(sampleRate_ * releaseSeconds * 4.0);
}

} // namespace vstengine::lead
