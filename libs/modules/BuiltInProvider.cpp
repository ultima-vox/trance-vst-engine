#include "modules/BuiltInProvider.h"
#include "bass/PsyBassVoice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace vstengine::modules {
namespace {
class BassInstance final : public instrument::InstrumentInstance {
public:
    bool prepare(const instrument::PrepareSpec& value) override
    {
        if (value.sampleRate <= 0.0 || value.maximumBlockSize == 0
            || value.outputChannels == 0 || value.outputChannels > 2)
            return false;
        spec = value;
        voice.prepareDirect(value.sampleRate);
        reset();
        return true;
    }
    void reset() noexcept override
    {
        voice.resetDirect();
    }
    void suspend() noexcept override { suspended = true; reset(); }
    void resume() noexcept override { suspended = false; }
    void setBypassed(bool value) noexcept override
    {
        if (bypassed == value) return;
        bypassed = value;
        if (bypassed) reset();
    }
    void process(const instrument::ProcessBlock& block) noexcept override
    {
        if (bypassed || suspended || block.sampleCount > spec.maximumBlockSize
            || block.outputs.empty()) return;
        updateVoice();
        juce::AudioBuffer<float> audio(block.outputs.data(),
            static_cast<int>(block.outputs.size()),
            static_cast<int>(block.sampleCount));
        std::uint32_t cursor = 0;
        for (const auto& event : block.midi) {
            if (event.sampleOffset >= block.sampleCount || event.size < 1) continue;
            if (event.sampleOffset > cursor)
                voice.renderNextBlock(audio, static_cast<int>(cursor),
                    static_cast<int>(event.sampleOffset - cursor));
            cursor = event.sampleOffset;
            const auto type = event.data[0] & 0xf0;
            if (event.size >= 3 && type == 0x90 && event.data[2] != 0)
                voice.noteOnDirect(event.data[1], event.data[2] / 127.0f);
            else if (event.size >= 3 && (type == 0x80 || type == 0x90))
                voice.noteOffDirect(true);
            else if (event.size >= 3 && type == 0xb0
                     && (event.data[1] == 120 || event.data[1] == 123))
                voice.resetDirect();
        }
        if (cursor < block.sampleCount)
            voice.renderNextBlock(audio, static_cast<int>(cursor),
                static_cast<int>(block.sampleCount - cursor));
    }
    bool setParameter(std::string_view id, float value) noexcept override
    {
        if (!std::isfinite(value)) return false;
        value = std::clamp(value, 0.0f, 1.0f);
        for (std::size_t i = 0; i < values.size(); ++i)
            if (ids[i] == id) { values[i] = value; return true; }
        return false;
    }
    bool loadState(std::uint32_t schema, std::span<const std::byte> payload) noexcept override
    {
        if (schema != instrument::stateSchemaVersion) return false;
        if (payload.empty()) return true;
        if (payload.size() != sizeof(values)) return false;
        std::memcpy(values.data(), payload.data(), sizeof(values));
        return std::all_of(values.begin(), values.end(), [](float value) {
            return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
        });
    }
    bool saveState(std::span<std::byte> destination,
                   std::uint32_t& written) const noexcept override
    {
        written = sizeof(values);
        if (destination.size() < written) return false;
        std::memcpy(destination.data(), values.data(), sizeof(values));
        return true;
    }
    std::uint32_t latencySamples() const noexcept override { return 0; }
    std::uint32_t tailSamples() const noexcept override
    {
        return static_cast<std::uint32_t>(spec.sampleRate * 0.25);
    }
private:
    void updateVoice() noexcept
    {
        voice.setFilterCutoff(0.05f + values[0] * 0.85f);
        voice.setFilterResonance(values[1]);
        voice.setPitchEnvelopeAmount(values[2] * 24.0f);
        voice.setAmpAttack(0.0002f + values[5] * 0.0012f);
        voice.setAmpDecay(0.015f + values[3] * 0.18f);
        voice.setAmpRelease(0.008f + values[3] * 0.08f);
        voice.setDrive(1.0f + values[4] * 5.0f);
        voice.setFilterDrive(1.0f + values[4] * 4.0f);
        voice.setAmpSustain(0.28f + values[5] * 0.45f);
        voice.setKeyTracking(0.1f + values[7] * 0.7f);
        voice.setOutputLevel(0.72f);
    }
    static constexpr std::array<std::string_view, 8> ids {
        "cutoff", "resonance", "envelope", "decay",
        "drive", "accent", "space", "motion"
    };
    std::array<float, 8> values { 0.48f, 0.55f, 0.4f, 0.25f,
                                  0.32f, 0.5f, 0.0f, 0.15f };
    bass::PsyBassVoice voice;
    instrument::PrepareSpec spec;
    bool bypassed {};
    bool suspended {};
};

class ReferencePluckInstance final : public instrument::InstrumentInstance {
public:
    bool prepare(const instrument::PrepareSpec& value) override
    {
        if (value.sampleRate <= 0.0 || value.maximumBlockSize == 0
            || value.outputChannels == 0 || value.outputChannels > 2)
            return false;
        spec = value;
        reset();
        return true;
    }
    void reset() noexcept override { phase = 0.0; envelope = 0.0f; active = false; }
    void suspend() noexcept override { suspended = true; reset(); }
    void resume() noexcept override { suspended = false; }
    void setBypassed(bool value) noexcept override
    {
        if (bypassed == value) return;
        bypassed = value;
        if (value) reset();
    }
    void process(const instrument::ProcessBlock& block) noexcept override
    {
        if (bypassed || suspended || block.sampleCount > spec.maximumBlockSize) return;
        std::size_t eventIndex = 0;
        for (std::uint32_t sample = 0; sample < block.sampleCount; ++sample) {
            while (eventIndex < block.midi.size()
                   && block.midi[eventIndex].sampleOffset == sample) {
                const auto& event = block.midi[eventIndex++];
                const auto type = event.data[0] & 0xf0;
                if (event.size >= 3 && type == 0x90 && event.data[2] != 0) {
                    frequency = 440.0 * std::pow(2.0,
                        (static_cast<int>(event.data[1]) - 69) / 12.0);
                    phase = 0.0;
                    envelope = event.data[2] / 127.0f;
                    active = true;
                } else if (event.size >= 3
                           && (type == 0x80 || type == 0x90)) {
                    active = false;
                }
            }
            const float wave = static_cast<float>(2.0 * std::abs(2.0 * phase - 1.0) - 1.0);
            const float value = active ? wave * envelope * level : 0.0f;
            for (auto* output : block.outputs) output[sample] += value;
            phase += frequency / spec.sampleRate;
            phase -= std::floor(phase);
            envelope *= active ? decay : 0.98f;
            if (envelope < 1.0e-5f) active = false;
        }
    }
    bool setParameter(std::string_view id, float value) noexcept override
    {
        if (!std::isfinite(value)) return false;
        value = std::clamp(value, 0.0f, 1.0f);
        if (id == "tone") { decay = 0.9990f + value * 0.0009f; return true; }
        if (id == "level") { level = value * 0.25f; return true; }
        return false;
    }
    bool loadState(std::uint32_t schema, std::span<const std::byte> payload) noexcept override
    {
        if (schema != instrument::stateSchemaVersion) return false;
        if (payload.empty()) return true;
        const std::array<float, 2>* values = nullptr;
        if (payload.size() != sizeof(std::array<float, 2>)) return false;
        std::array<float, 2> decoded {};
        std::memcpy(decoded.data(), payload.data(), sizeof(decoded));
        values = &decoded;
        if (!std::isfinite((*values)[0]) || !std::isfinite((*values)[1])
            || (*values)[0] < 0.0f || (*values)[0] > 1.0f
            || (*values)[1] < 0.0f || (*values)[1] > 1.0f) return false;
        setParameter("tone", (*values)[0]);
        setParameter("level", (*values)[1]);
        return true;
    }
    bool saveState(std::span<std::byte> destination,
                   std::uint32_t& written) const noexcept override
    {
        const std::array<float, 2> values {
            (decay - 0.9990f) / 0.0009f, level / 0.25f
        };
        written = sizeof(values);
        if (destination.size() < written) return false;
        std::memcpy(destination.data(), values.data(), sizeof(values));
        return true;
    }
    std::uint32_t latencySamples() const noexcept override { return 0; }
    std::uint32_t tailSamples() const noexcept override
    {
        return static_cast<std::uint32_t>(spec.sampleRate * 0.5);
    }
private:
    instrument::PrepareSpec spec;
    double phase {};
    double frequency { 220.0 };
    float envelope {};
    float decay { 0.9995f };
    float level { 0.15f };
    bool active {};
    bool bypassed {};
    bool suspended {};
};

class BuiltInProvider final : public instrument::InstrumentProvider {
public:
    BuiltInProvider()
    {
        instrument::InstrumentDescriptor bassDescriptor;
        bassDescriptor.id = bassInstrumentId;
        bassDescriptor.providerId = "com.ultimavox.builtin";
        bassDescriptor.name = "Psy Bass";
        bassDescriptor.vendor = "Ultima Vox";
        bassDescriptor.capabilities = instrument::Capability::notes
            | instrument::Capability::sequence
            | instrument::Capability::patternGenerator
            | instrument::Capability::modulation
            | instrument::Capability::standardEditor;
        bassDescriptor.budget = { 1, 2048, 65536, 0, 0, 24000,
                                  2048, 64, 131072 };
        for (const auto id : { "cutoff", "resonance", "envelope", "decay",
                               "drive", "accent", "space", "motion" })
        {
            const auto macro = static_cast<std::int8_t>(
                bassDescriptor.parameters.size());
            bassDescriptor.parameters.push_back({ id, id, "", 0.0f, 1.0f,
                0.5f, 0.0001f, instrument::ParameterType::floating, "Sound",
                {}, true, true, macro });
        }
        descriptors_.push_back(std::move(bassDescriptor));

        instrument::InstrumentDescriptor reference;
        reference.id = referenceInstrumentId;
        reference.providerId = "com.ultimavox.builtin";
        reference.name = "Reference Pluck";
        reference.vendor = "Ultima Vox";
        reference.budget = { 1, 2048, 4096, 0, 0, 48000,
                             2048, 8, 131072 };
        reference.parameters = {
            { "tone", "Tone", "", 0.0f, 1.0f, 0.5f, 0.0001f,
              instrument::ParameterType::floating, "Sound", {}, true, true, 0 },
            { "level", "Level", "", 0.0f, 1.0f, 0.6f, 0.0001f,
              instrument::ParameterType::floating, "Output", {}, true, true, 1 }
        };
        descriptors_.push_back(std::move(reference));
    }
    std::span<const instrument::InstrumentDescriptor>
        descriptors() const noexcept override { return descriptors_; }
    std::unique_ptr<instrument::InstrumentInstance> create(
        std::string_view id, const instrument::CreateContext&) override
    {
        if (id == bassInstrumentId) return std::make_unique<BassInstance>();
        if (id == referenceInstrumentId)
            return std::make_unique<ReferencePluckInstance>();
        return {};
    }
private:
    std::vector<instrument::InstrumentDescriptor> descriptors_;
};
} // namespace

std::unique_ptr<instrument::InstrumentProvider> createBuiltInProvider()
{
    return std::make_unique<BuiltInProvider>();
}
} // namespace vstengine::modules
