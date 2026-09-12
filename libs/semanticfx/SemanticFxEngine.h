#pragma once

#include "instrument/InstrumentAbi.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace vstengine::semanticfx {

enum class EventFamily : std::uint8_t {
    sweep,
    laser,
    riser,
    downlifter,
    impact,
    whoosh,
    zap,
    noiseBurst,
    metallic,
    alien,
    count
};

class SemanticFxEngine final {
public:
    static constexpr std::uint32_t stateVersion = 1;
    static constexpr std::size_t parameterCount = 8;
    static constexpr std::size_t encodedStateBytes = 8 + parameterCount * 4;
    static constexpr std::size_t maximumVoices = 4;

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
    [[nodiscard]] std::size_t activeVoiceCount() const noexcept;

private:
    enum Parameter : std::size_t {
        family, tone, duration, brightness, motion, texture, drive, output,
        count
    };

    struct Voice {
        EventFamily event { EventFamily::sweep };
        std::uint32_t age {};
        std::uint32_t length { 1 };
        std::uint32_t randomState { 1 };
        double phaseA {};
        double phaseB {};
        float lowpass {};
        float previousLowpass {};
        float velocity { 1.0f };
        float noteRatio { 1.0f };
        bool active {};
    };

    void trigger(std::uint8_t note, std::uint8_t velocity) noexcept;
    void allNotesOff() noexcept;
    [[nodiscard]] float renderVoice(Voice& voice) noexcept;
    [[nodiscard]] float noise(Voice& voice) noexcept;
    [[nodiscard]] static float smoothEnvelope(float phase) noexcept;

    static constexpr std::array<std::string_view, parameterCount> parameterIds {
        "family", "tone", "duration", "brightness",
        "motion", "texture", "drive", "output"
    };
    std::array<float, parameterCount> targets_ {
        0.444444f, 0.42f, 0.38f, 0.62f, 0.55f, 0.35f, 0.28f, 0.72f
    };
    std::array<float, parameterCount> smoothed_ = targets_;
    std::array<Voice, maximumVoices> voices_ {};
    double sampleRate_ { 48000.0 };
    std::uint32_t maximumBlockSize_ {};
    std::uint32_t outputChannels_ {};
    std::uint32_t triggerOrdinal_ {};
    bool prepared_ {};
};

} // namespace vstengine::semanticfx
