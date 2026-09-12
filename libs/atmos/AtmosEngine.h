#pragma once

#include "instrument/InstrumentAbi.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace vstengine::atmos {

class AtmosEngine final {
public:
    static constexpr std::uint32_t stateVersion = 1;
    static constexpr std::size_t parameterCount = 16;
    static constexpr std::size_t encodedStateBytes = 8 + parameterCount * 4;
    static constexpr std::size_t maximumVoices = 6;
    static constexpr std::size_t oscillatorCount = 4;

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
        blend, brightness, motion, evolution, attack, release, space, output,
        spread, drift, resonance, texture, density, harmonics, decay, sustain,
        count
    };
    enum class EnvelopeStage : std::uint8_t {
        off, attack, decay, sustain, release
    };
    struct Voice {
        std::array<double, oscillatorCount> phase {};
        std::array<double, oscillatorCount> phaseRight {};
        double slowPhase {};
        double evolutionPhase {};
        float envelope {};
        float filterEnvelope {};
        float lowLeft {};
        float lowRight {};
        float bandLeft {};
        float bandRight {};
        float diffusionLeft {};
        float diffusionRight {};
        float velocity {};
        float pan {};
        std::uint64_t age {};
        std::uint32_t randomState { 1 };
        std::uint8_t note { 255 };
        EnvelopeStage stage { EnvelopeStage::off };
        bool active {};
    };

    void noteOn(std::uint8_t note, std::uint8_t velocity) noexcept;
    void noteOff(std::uint8_t note) noexcept;
    void allNotesOff() noexcept;
    void updateEnvelope(Voice& voice) noexcept;
    [[nodiscard]] float renderOscillator(double& phase, double increment,
                                         float shape) noexcept;
    [[nodiscard]] float filtered(Voice& voice, float input, float cutoffHz,
                                 bool right) noexcept;
    [[nodiscard]] float noise(Voice& voice) noexcept;
    [[nodiscard]] static double midiFrequency(float note) noexcept;
    [[nodiscard]] static float polyBlep(double phase, double increment) noexcept;

    static constexpr std::array<std::string_view, parameterCount> parameterIds {
        "blend", "brightness", "motion", "evolution", "attack", "release",
        "space", "output", "spread", "drift", "resonance", "texture",
        "density", "harmonics", "decay", "sustain"
    };
    std::array<float, parameterCount> targets_ {
        0.34f, 0.30f, 0.58f, 0.72f, 0.64f, 0.86f, 0.78f, 0.70f,
        0.74f, 0.52f, 0.48f, 0.58f, 0.72f, 0.42f, 0.78f, 0.88f
    };
    std::array<float, parameterCount> smoothed_ = targets_;
    std::array<Voice, maximumVoices> voices_ {};
    double sampleRate_ { 48000.0 };
    std::uint32_t maximumBlockSize_ {};
    std::uint32_t outputChannels_ {};
    std::uint64_t voiceAge_ {};
    bool prepared_ {};
};

} // namespace vstengine::atmos
