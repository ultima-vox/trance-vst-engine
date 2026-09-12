#include "support/ReferenceInstrument.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace vstengine::tests {
namespace {

class ReferenceInstrument final : public instrument::InstrumentInstance {
public:
    bool prepare(const instrument::PrepareSpec& next) override
    {
        if (next.sampleRate <= 0.0 || next.maximumBlockSize == 0
            || next.outputChannels == 0 || next.outputChannels > 2)
            return false;
        spec_ = next;
        prepared_ = true;
        reset();
        return true;
    }

    void reset() noexcept override
    {
        phase_ = 0.0;
        envelope_ = 0.0f;
        frequency_ = 261.6255653005986;
        active_ = false;
    }

    void suspend() noexcept override
    {
        suspended_ = true;
        reset();
    }

    void resume() noexcept override { suspended_ = false; }

    void setBypassed(bool bypassed) noexcept override
    {
        bypassed_ = bypassed;
        if (bypassed_) reset();
    }

    void process(const instrument::ProcessBlock& block) noexcept override
    {
        if (!prepared_ || suspended_ || bypassed_
            || block.sampleCount > spec_.maximumBlockSize
            || block.outputs.empty())
            return;

        std::size_t eventIndex = 0;
        for (std::uint32_t sample = 0; sample < block.sampleCount; ++sample) {
            while (eventIndex < block.midi.size()
                   && block.midi[eventIndex].sampleOffset <= sample) {
                handleEvent(block.midi[eventIndex]);
                ++eventIndex;
            }
            const float triangle = static_cast<float>(
                2.0 * std::abs(2.0 * phase_ - 1.0) - 1.0);
            const float value = active_ ? triangle * envelope_ * level_ : 0.0f;
            for (float* output : block.outputs) output[sample] += value;
            phase_ += frequency_ / spec_.sampleRate;
            phase_ -= std::floor(phase_);
            envelope_ *= decay_;
            if (envelope_ < 1.0e-6f) active_ = false;
        }
    }

    bool setParameter(std::string_view id, float value) noexcept override
    {
        if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            return false;
        if (id == "tone") {
            tone_ = value;
            decay_ = 0.985f + value * 0.0145f;
            return true;
        }
        if (id == "level") {
            level_ = value * 0.2f;
            return true;
        }
        return false;
    }

    bool loadState(std::uint32_t schemaVersion,
                   std::span<const std::byte> payload) noexcept override
    {
        if (schemaVersion != instrument::stateSchemaVersion) return false;
        if (payload.empty()) return true;
        if (payload.size() != sizeof(State)) return false;
        State decoded;
        std::memcpy(&decoded, payload.data(), sizeof(decoded));
        if (!std::isfinite(decoded.tone) || !std::isfinite(decoded.level)
            || decoded.tone < 0.0f || decoded.tone > 1.0f
            || decoded.level < 0.0f || decoded.level > 1.0f)
            return false;
        return setParameter("tone", decoded.tone)
            && setParameter("level", decoded.level);
    }

    bool saveState(std::span<std::byte> destination,
                   std::uint32_t& bytesWritten) const noexcept override
    {
        const State state { tone_, level_ / 0.2f };
        bytesWritten = sizeof(state);
        if (destination.size() < bytesWritten) return false;
        std::memcpy(destination.data(), &state, sizeof(state));
        return true;
    }

    std::uint32_t latencySamples() const noexcept override { return 0; }

    std::uint32_t tailSamples() const noexcept override
    {
        return prepared_ ? static_cast<std::uint32_t>(spec_.sampleRate * 0.5)
                         : 0;
    }

private:
    struct State {
        float tone;
        float level;
    };

    void handleEvent(const VoxMidiEventV1& event) noexcept
    {
        if (event.size < 1) return;
        const auto type = event.data[0] & 0xf0;
        if (event.size >= 3 && type == 0x90 && event.data[2] != 0) {
            frequency_ = 440.0 * std::pow(
                2.0, (static_cast<int>(event.data[1]) - 69) / 12.0);
            phase_ = 0.0;
            envelope_ = event.data[2] / 127.0f;
            active_ = true;
        } else if (event.size >= 3
                   && (type == 0x80 || (type == 0x90 && event.data[2] == 0))) {
            active_ = false;
        } else if (event.size >= 3 && type == 0xb0
                   && (event.data[1] == 120 || event.data[1] == 123)) {
            reset();
        }
    }

    instrument::PrepareSpec spec_;
    double phase_ {};
    double frequency_ { 261.6255653005986 };
    float envelope_ {};
    float tone_ { 0.5f };
    float decay_ { 0.99225f };
    float level_ { 0.12f };
    bool active_ {};
    bool prepared_ {};
    bool suspended_ {};
    bool bypassed_ {};
};

class ReferenceProvider final : public instrument::InstrumentProvider {
public:
    ReferenceProvider()
    {
        descriptor_.id = referenceInstrumentId;
        descriptor_.providerId = "com.ultimavox.test-provider";
        descriptor_.name = "Reference Instrument";
        descriptor_.vendor = "Ultima Vox";
        descriptor_.capabilities = instrument::Capability::notes
            | instrument::Capability::standardEditor;
        descriptor_.tailPolicy = instrument::TailPolicy::bounded;
        descriptor_.budget = { 1, 2048, 64, 0, 0, 48000,
                               0, 0, 4096 };
        descriptor_.parameters = {
            { "tone", "Tone", "", 0.0f, 1.0f, 0.5f, 0.0001f,
              instrument::ParameterType::floating, "Sound", {}, true, true, 0 },
            { "level", "Level", "", 0.0f, 1.0f, 0.6f, 0.0001f,
              instrument::ParameterType::floating, "Output", {}, true, false, 1 }
        };
    }

    std::span<const instrument::InstrumentDescriptor>
    descriptors() const noexcept override
    {
        return { &descriptor_, 1 };
    }

    std::unique_ptr<instrument::InstrumentInstance> create(
        std::string_view id, const instrument::CreateContext&) override
    {
        if (id != referenceInstrumentId) return {};
        return std::make_unique<ReferenceInstrument>();
    }

private:
    instrument::InstrumentDescriptor descriptor_;
};

} // namespace

std::unique_ptr<instrument::InstrumentProvider>
createReferenceInstrumentProvider()
{
    return std::make_unique<ReferenceProvider>();
}

} // namespace vstengine::tests
