#pragma once

#include "instrument/InstrumentAbi.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace vstengine::lead {

class LeadEngine final {
public:
    static constexpr std::uint32_t stateVersion = 1;
    static constexpr std::size_t parameterCount = 16;
    static constexpr std::size_t encodedStateBytes = 8 + parameterCount * 4;
    static constexpr std::size_t maximumVoices = 8;
    static constexpr std::size_t maximumUnison = 5;

    bool prepare(double sampleRate, std::uint32_t maximumBlockSize,
                 std::uint32_t outputChannels) noexcept;
    void reset() noexcept;
    void process(std::span<float*> outputs, std::uint32_t sampleCount,
                 std::span<const VoxMidiEventV1> midi) noexcept;

    bool setParameter(std::string_view id, float normalizedValue) noexcept;
    [[nodiscard]] float parameter(std::string_view id) const noexcept;
    bool loadState(std::uint32_t schemaVersion,
                   std::span<const std::byte> payload) noexcept;
    bool saveState(std::span<std::byte> destination,
                   std::uint32_t& bytesWritten) const noexcept;

    [[nodiscard]] std::uint32_t latencySamples() const noexcept { return 0; }
    [[nodiscard]] std::uint32_t tailSamples() const noexcept;

private:
    enum Parameter : std::size_t {
        morph, cutoff, resonance, filterEnvelope, attack, release, drive,
        output, sync, fm, ring, noise, unison, detune, decay, sustain, count
    };
    enum class EnvelopeStage : std::uint8_t { off, attack, decay, sustain, release };
    struct Voice {
        std::array<double, maximumUnison> phase {};
        std::array<double, maximumUnison> modPhase {};
        float ampEnvelope {};
        float filterEnvelope {};
        float filterIntegrator1 {};
        float filterIntegrator2 {};
        float velocity {};
        std::uint64_t age {};
        std::uint32_t noiseState { 1 };
        std::uint8_t note { 255 };
        EnvelopeStage stage { EnvelopeStage::off };
        bool gate {};
        bool active {};
    };

    void noteOn(std::uint8_t note, std::uint8_t velocity) noexcept;
    void noteOff(std::uint8_t note) noexcept;
    void allNotesOff() noexcept;
    void pitchBend(std::uint8_t lsb, std::uint8_t msb) noexcept;
    [[nodiscard]] float oscillator(Voice& voice, std::size_t oscillator,
                                   double increment, float detunePosition) noexcept;
    [[nodiscard]] float filter(Voice& voice, float input, float cutoffHz,
                               float resonanceAmount) noexcept;
    void updateEnvelope(Voice& voice) noexcept;
    [[nodiscard]] static double midiFrequency(float note) noexcept;
    [[nodiscard]] static float polyBlep(double phase, double increment) noexcept;

    static constexpr std::array<std::string_view, parameterCount> parameterIds {
        "morph", "cutoff", "resonance", "filter-envelope", "attack",
        "release", "drive", "output", "sync", "fm", "ring", "noise",
        "unison", "detune", "decay", "sustain"
    };
    std::array<float, parameterCount> targets_ {
        0.18f, 0.62f, 0.28f, 0.58f, 0.04f, 0.25f, 0.20f, 0.70f,
        0.04f, 0.05f, 0.00f, 0.01f, 0.38f, 0.22f, 0.30f, 0.70f
    };
    std::array<float, parameterCount> smoothed_ = targets_;
    std::array<Voice, maximumVoices> voices_ {};
    double sampleRate_ { 48000.0 };
    std::uint32_t maximumBlockSize_ {};
    std::uint32_t outputChannels_ {};
    float pitchBendSemitones_ {};
    std::uint64_t voiceAge_ {};
    bool prepared_ {};
};

} // namespace vstengine::lead
