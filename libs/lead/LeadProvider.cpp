#include "lead/LeadProvider.h"
#include "lead/LeadEngine.h"
#include "lead/LeadGenerator.h"
#include <algorithm>
#include <array>

namespace vstengine::lead {
namespace {
class LeadInstance final : public instrument::InstrumentInstance {
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
        if (!bypassed_ && bypassed) engine_.reset();
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
    LeadEngine engine_;
    bool bypassed_ {};
    bool suspended_ {};
};

struct SoundPreset {
    const char* id;
    const char* name;
    std::array<float, LeadEngine::parameterCount> values;
};

constexpr std::array soundPresets {
    SoundPreset { "psy-arp", "Psy Arp",
        { 0.18f, 0.62f, 0.28f, 0.58f, 0.04f, 0.25f, 0.20f, 0.70f,
          0.04f, 0.05f, 0.00f, 0.01f, 0.38f, 0.22f, 0.30f, 0.70f } },
    SoundPreset { "forest-call", "Forest Call",
        { 0.42f, 0.44f, 0.48f, 0.76f, 0.14f, 0.52f, 0.38f, 0.67f,
          0.14f, 0.18f, 0.06f, 0.08f, 0.18f, 0.12f, 0.56f, 0.62f } },
    SoundPreset { "alien-phrase", "Alien Phrase",
        { 0.70f, 0.55f, 0.61f, 0.82f, 0.06f, 0.42f, 0.48f, 0.62f,
          0.58f, 0.52f, 0.34f, 0.12f, 0.28f, 0.19f, 0.42f, 0.54f } },
    SoundPreset { "metallic-sequence", "Metallic Sequence",
        { 0.84f, 0.68f, 0.36f, 0.46f, 0.01f, 0.18f, 0.64f, 0.56f,
          0.78f, 0.64f, 0.58f, 0.03f, 0.12f, 0.08f, 0.18f, 0.42f } },
    SoundPreset { "hi-tech-burst", "Hi-Tech Burst",
        { 0.28f, 0.74f, 0.22f, 0.88f, 0.01f, 0.12f, 0.76f, 0.52f,
          0.46f, 0.70f, 0.18f, 0.18f, 0.50f, 0.40f, 0.12f, 0.34f } },
    SoundPreset { "hypnotic-lead", "Hypnotic Lead",
        { 0.54f, 0.39f, 0.72f, 0.50f, 0.28f, 0.82f, 0.26f, 0.65f,
          0.20f, 0.20f, 0.08f, 0.02f, 0.72f, 0.48f, 0.68f, 0.80f } }
};

LeadProfile profile(std::string_view id) noexcept
{
    if (id == "forest-call") return LeadProfile::forestCall;
    if (id == "alien-phrase") return LeadProfile::alienPhrase;
    if (id == "metallic-sequence") return LeadProfile::metallicSequence;
    if (id == "hi-tech-burst") return LeadProfile::hiTechBurst;
    if (id == "hypnotic-lead") return LeadProfile::hypnoticLead;
    return LeadProfile::psyArp;
}

class LeadProvider final : public instrument::InstrumentProvider {
public:
    LeadProvider()
    {
        descriptor_.id = std::string(instrumentId);
        descriptor_.providerId = "com.ultimavox.builtin";
        descriptor_.name = "Lead";
        descriptor_.vendor = "Ultima Vox";
        descriptor_.instrumentVersion = 1;
        descriptor_.stateVersion = LeadEngine::stateVersion;
        descriptor_.contentVersion = 1;
        descriptor_.capabilities = instrument::Capability::notes
            | instrument::Capability::sequence
            | instrument::Capability::patternGenerator
            | instrument::Capability::modulation
            | instrument::Capability::standardEditor;
        descriptor_.tailPolicy = instrument::TailPolicy::bounded;
        descriptor_.budget = { static_cast<std::uint32_t>(LeadEngine::maximumVoices),
            2048, 4096, 0, 0, 1536000, 64, 32, 65536 };
        constexpr std::array ids { "morph", "cutoff", "resonance",
            "filter-envelope", "attack", "release", "drive", "output",
            "sync", "fm", "ring", "noise", "unison", "detune", "decay",
            "sustain" };
        constexpr std::array names { "Osc Morph", "Cutoff", "Resonance",
            "Filter Envelope", "Attack", "Release", "Drive", "Output",
            "Hard Sync", "FM", "Ring Mod", "Noise", "Unison", "Detune",
            "Decay", "Sustain" };
        constexpr std::array groups { "Oscillator", "Filter", "Filter",
            "Filter", "Envelope", "Envelope", "Character", "Output",
            "Oscillator", "Oscillator", "Oscillator", "Oscillator",
            "Oscillator", "Oscillator", "Envelope", "Envelope" };
        for (std::size_t i = 0; i < ids.size(); ++i)
            descriptor_.parameters.push_back({ ids[i], names[i], "", 0.0f,
                1.0f, soundPresets[0].values[i], 0.0001f,
                instrument::ParameterType::floating, groups[i], {}, true, true,
                i < instrument::macrosPerSlot ? static_cast<std::int8_t>(i) : -1 });

        for (const auto& preset : soundPresets) {
            instrument::ContentDescriptor sound { std::string(instrumentId),
                preset.id, preset.name, instrument::ContentKind::soundPreset,
                1, 1, VOX_SEQUENCE_ALL, VOX_PRESET_CLASS_SOUND, 0xffu };
            std::copy_n(preset.values.begin(), instrument::macrosPerSlot,
                        sound.macroValues.begin());
            content_.push_back(std::move(sound));
            content_.push_back({ std::string(instrumentId), preset.id,
                preset.name, instrument::ContentKind::patternPreset, 1, 1,
                VOX_SEQUENCE_ALL, VOX_PRESET_CLASS_PATTERN });
            content_.push_back({ std::string(instrumentId), preset.id,
                preset.name, instrument::ContentKind::generatorProfile, 1, 1,
                VOX_SEQUENCE_ALL, VOX_PRESET_CLASS_GENERATOR });
        }
    }

    std::span<const instrument::InstrumentDescriptor>
        descriptors() const noexcept override { return { &descriptor_, 1 }; }

    std::unique_ptr<instrument::InstrumentInstance> create(
        std::string_view id, const instrument::CreateContext&) override
    {
        return id == instrumentId ? std::make_unique<LeadInstance>() : nullptr;
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
        for (const auto& preset : soundPresets)
            if (presetId == preset.id) {
                LeadEngine candidate;
                for (std::size_t i = 0; i < preset.values.size(); ++i)
                    if (!candidate.setParameter(descriptor_.parameters[i].id,
                                                preset.values[i]))
                        return instrument::ContentStatus::invalidArgument;
                std::array<std::byte, LeadEngine::encodedStateBytes> payload {};
                std::uint32_t written {};
                if (!candidate.saveState(payload, written)
                    || written != payload.size())
                    return instrument::ContentStatus::invalidArgument;
                return instance.loadState(LeadEngine::stateVersion, payload)
                    ? instrument::ContentStatus::ok
                    : instrument::ContentStatus::incompatible;
            }
        return instrument::ContentStatus::notFound;
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
        const auto found = std::find_if(soundPresets.begin(), soundPresets.end(),
            [profileId](const auto& preset) { return profileId == preset.id; });
        if (found == soundPresets.end()) return instrument::ContentStatus::notFound;
        const auto generated = LeadGenerator::generate(profile(profileId),
            context.globalSeed, context.slotId, instrumentId);
        VoxPatternV1 candidate {};
        candidate.structSize = sizeof(candidate);
        candidate.schemaVersion = VOX_PATTERN_SCHEMA_V1;
        candidate.stepCount = static_cast<std::uint32_t>(generated.size());
        candidate.timingMode = static_cast<std::uint32_t>(generated.getTimingMode());
        for (std::uint32_t i = 0; i < candidate.stepCount; ++i) {
            const auto& source = generated[static_cast<int>(i)];
            auto& target = candidate.steps[i];
            target.noteOffset = static_cast<std::int16_t>(source.noteOffset);
            target.gate = source.gate ? 1 : 0;
            target.accent = source.accent ? 1 : 0;
            target.velocity = source.velocity;
            target.probability = source.probability;
            target.ratchetCount = static_cast<std::uint8_t>(source.ratchetCount);
            target.slideDuration = source.slideDuration;
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

std::unique_ptr<instrument::InstrumentProvider> createLeadProvider()
{
    return std::make_unique<LeadProvider>();
}

} // namespace vstengine::lead
