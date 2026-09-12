#include "atmos/AtmosProvider.h"
#include "atmos/AtmosEngine.h"
#include "atmos/AtmosGenerator.h"
#include <algorithm>
#include <array>

namespace vstengine::atmos {
namespace {
class AtmosInstance final : public instrument::InstrumentInstance {
public:
    bool prepare(const instrument::PrepareSpec& spec) override
    {
        return engine_.prepare(spec.sampleRate, spec.maximumBlockSize,
                               spec.outputChannels);
    }
    void reset() noexcept override { engine_.reset(); }
    void suspend() noexcept override { suspended_ = true; engine_.reset(); }
    void resume() noexcept override { suspended_ = false; }
    void setBypassed(bool bypassed) noexcept override
    {
        if (bypassed && bypassed_ != bypassed) engine_.reset();
        bypassed_ = bypassed;
    }
    void process(const instrument::ProcessBlock& block) noexcept override
    {
        if (!bypassed_ && !suspended_)
            engine_.process(block.outputs, block.sampleCount, block.midi);
    }
    bool setParameter(std::string_view id, float value) noexcept override
    {
        return engine_.setParameter(id, value);
    }
    bool loadState(std::uint32_t schema,
                   std::span<const std::byte> payload) noexcept override
    {
        return engine_.loadState(schema, payload);
    }
    bool saveState(std::span<std::byte> destination,
                   std::uint32_t& written) const noexcept override
    {
        return engine_.saveState(destination, written);
    }
    std::uint32_t latencySamples() const noexcept override
    {
        return engine_.latencySamples();
    }
    std::uint32_t tailSamples() const noexcept override
    {
        return engine_.tailSamples();
    }
private:
    AtmosEngine engine_;
    bool bypassed_ {};
    bool suspended_ {};
};

struct Preset {
    const char* id;
    const char* name;
    AtmosProfile profile;
    std::array<float, AtmosEngine::parameterCount> values;
};

constexpr std::array presets {
    Preset { "deep-space", "Deep Space", AtmosProfile::deepSpace,
        { 0.34f, 0.30f, 0.58f, 0.72f, 0.64f, 0.86f, 0.78f, 0.70f,
          0.74f, 0.52f, 0.48f, 0.58f, 0.72f, 0.42f, 0.78f, 0.88f } },
    Preset { "forest-bed", "Forest Bed", AtmosProfile::forestBed,
        { 0.52f, 0.43f, 0.68f, 0.58f, 0.45f, 0.72f, 0.64f, 0.66f,
          0.58f, 0.66f, 0.36f, 0.78f, 0.46f, 0.35f, 0.64f, 0.74f } },
    Preset { "alien-drone", "Alien Drone", AtmosProfile::alienDrone,
        { 0.72f, 0.38f, 0.84f, 0.92f, 0.70f, 0.90f, 0.72f, 0.62f,
          0.88f, 0.78f, 0.68f, 0.62f, 0.82f, 0.75f, 0.82f, 0.92f } },
    Preset { "crystal-air", "Crystal Air", AtmosProfile::crystalAir,
        { 0.18f, 0.78f, 0.42f, 0.66f, 0.34f, 0.76f, 0.88f, 0.60f,
          0.92f, 0.22f, 0.24f, 0.28f, 0.62f, 0.86f, 0.52f, 0.68f } },
    Preset { "dark-ritual", "Dark Ritual", AtmosProfile::darkRitual,
        { 0.62f, 0.20f, 0.48f, 0.80f, 0.56f, 0.82f, 0.54f, 0.72f,
          0.44f, 0.58f, 0.76f, 0.82f, 0.68f, 0.64f, 0.70f, 0.84f } },
    Preset { "evolving-cloud", "Evolving Cloud", AtmosProfile::evolvingCloud,
        { 0.46f, 0.56f, 0.76f, 0.96f, 0.52f, 0.80f, 0.82f, 0.64f,
          0.82f, 0.48f, 0.42f, 0.52f, 0.90f, 0.58f, 0.86f, 0.80f } }
};

class AtmosProvider final : public instrument::InstrumentProvider {
public:
    AtmosProvider()
    {
        descriptor_.id = std::string(instrumentId);
        descriptor_.providerId = "com.ultimavox.builtin";
        descriptor_.name = "Atmos / Texture";
        descriptor_.vendor = "Ultima Vox";
        descriptor_.instrumentVersion = 1;
        descriptor_.stateVersion = AtmosEngine::stateVersion;
        descriptor_.contentVersion = 1;
        descriptor_.capabilities = instrument::Capability::notes
            | instrument::Capability::sequence
            | instrument::Capability::patternGenerator
            | instrument::Capability::modulation
            | instrument::Capability::standardEditor;
        descriptor_.tailPolicy = instrument::TailPolicy::bounded;
        descriptor_.budget = {
            static_cast<std::uint32_t>(AtmosEngine::maximumVoices),
            2048, 4096, 0, 0, 1536000, 64, 32, 0
        };
        constexpr std::array ids { "blend", "brightness", "motion",
            "evolution", "attack", "release", "space", "output", "spread",
            "drift", "resonance", "texture", "density", "harmonics",
            "decay", "sustain" };
        constexpr std::array names { "Blend", "Brightness", "Motion",
            "Evolution", "Attack", "Release", "Space", "Output", "Spread",
            "Drift", "Resonance", "Texture", "Density", "Harmonics",
            "Decay", "Sustain" };
        constexpr std::array groups { "Source", "Filter", "Movement",
            "Movement", "Envelope", "Envelope", "Space", "Output", "Space",
            "Movement", "Filter", "Source", "Source", "Source", "Envelope",
            "Envelope" };
        for (std::size_t index = 0; index < ids.size(); ++index)
            descriptor_.parameters.push_back({ ids[index], names[index], "",
                0.0f, 1.0f, presets[0].values[index], 0.0001f,
                instrument::ParameterType::floating, groups[index], {}, true,
                true, index < instrument::macrosPerSlot
                    ? static_cast<std::int8_t>(index) : -1 });

        constexpr std::uint32_t fields = VOX_SEQUENCE_GATE | VOX_SEQUENCE_NOTE
            | VOX_SEQUENCE_VELOCITY | VOX_SEQUENCE_ACCENT
            | VOX_SEQUENCE_PROBABILITY | VOX_SEQUENCE_GATE_WIDTH;
        for (const auto& preset : presets) {
            instrument::ContentDescriptor sound { std::string(instrumentId),
                preset.id, preset.name, instrument::ContentKind::soundPreset,
                1, 1, fields, VOX_PRESET_CLASS_SOUND, 0xffu };
            std::copy_n(preset.values.begin(), instrument::macrosPerSlot,
                        sound.macroValues.begin());
            content_.push_back(std::move(sound));
            content_.push_back({ std::string(instrumentId), preset.id,
                preset.name, instrument::ContentKind::patternPreset, 1, 1,
                fields, VOX_PRESET_CLASS_PATTERN });
            content_.push_back({ std::string(instrumentId), preset.id,
                preset.name, instrument::ContentKind::generatorProfile, 1, 1,
                fields, VOX_PRESET_CLASS_GENERATOR });
        }
    }

    std::span<const instrument::InstrumentDescriptor>
        descriptors() const noexcept override { return { &descriptor_, 1 }; }

    std::unique_ptr<instrument::InstrumentInstance> create(
        std::string_view id, const instrument::CreateContext&) override
    {
        return id == instrumentId
            ? std::make_unique<AtmosInstance>() : nullptr;
    }

    std::span<const instrument::ContentDescriptor> contentDescriptors(
        std::string_view id) const noexcept override
    {
        return id == instrumentId
            ? std::span<const instrument::ContentDescriptor>(content_)
            : std::span<const instrument::ContentDescriptor> {};
    }

    instrument::ContentStatus applySoundPreset(
        std::string_view id, std::string_view presetId,
        instrument::InstrumentInstance& instance) const noexcept override
    {
        if (id != instrumentId) return instrument::ContentStatus::notFound;
        const auto found = std::find_if(presets.begin(), presets.end(),
            [presetId](const Preset& preset) { return presetId == preset.id; });
        if (found == presets.end()) return instrument::ContentStatus::notFound;
        AtmosEngine candidate;
        for (std::size_t index = 0; index < found->values.size(); ++index)
            if (!candidate.setParameter(descriptor_.parameters[index].id,
                                        found->values[index]))
                return instrument::ContentStatus::invalidArgument;
        std::array<std::byte, AtmosEngine::encodedStateBytes> state {};
        std::uint32_t written {};
        if (!candidate.saveState(state, written) || written != state.size())
            return instrument::ContentStatus::invalidArgument;
        return instance.loadState(AtmosEngine::stateVersion, state)
            ? instrument::ContentStatus::ok
            : instrument::ContentStatus::incompatible;
    }

    instrument::ContentStatus generatePattern(
        std::string_view id, std::string_view profileId,
        const VoxGenerationContextV1& context, const VoxPatternV1*,
        VoxPatternV1& output) const noexcept override
    {
        if (id != instrumentId) return instrument::ContentStatus::notFound;
        if (context.structSize != sizeof(VoxGenerationContextV1)
            || output.structSize != sizeof(VoxPatternV1))
            return instrument::ContentStatus::invalidArgument;
        const auto found = std::find_if(presets.begin(), presets.end(),
            [profileId](const Preset& preset) { return profileId == preset.id; });
        if (found == presets.end()) return instrument::ContentStatus::notFound;
        const auto generated = AtmosGenerator::generate(found->profile,
            context.globalSeed, context.slotId, instrumentId);
        VoxPatternV1 candidate {};
        candidate.structSize = sizeof(candidate);
        candidate.schemaVersion = VOX_PATTERN_SCHEMA_V1;
        candidate.stepCount = static_cast<std::uint32_t>(generated.size());
        candidate.timingMode = static_cast<std::uint32_t>(
            generated.getTimingMode());
        for (std::uint32_t index = 0; index < candidate.stepCount; ++index) {
            const auto& source = generated[static_cast<int>(index)];
            auto& target = candidate.steps[index];
            target.noteOffset = static_cast<std::int16_t>(source.noteOffset);
            target.gate = source.gate ? 1 : 0;
            target.accent = source.accent ? 1 : 0;
            target.velocity = source.velocity;
            target.probability = source.probability;
            target.ratchetCount = 1;
            target.gateWidth = source.gateWidth;
        }
        output = candidate;
        return instrument::ContentStatus::ok;
    }

private:
    instrument::InstrumentDescriptor descriptor_;
    std::vector<instrument::ContentDescriptor> content_;
};
} // namespace

std::unique_ptr<instrument::InstrumentProvider> createAtmosProvider()
{
    return std::make_unique<AtmosProvider>();
}

} // namespace vstengine::atmos
