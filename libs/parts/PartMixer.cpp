#include "parts/PartMixer.h"

namespace vstengine::parts {

bool PartMixer::isAudible(const Part& part, const bool anySolo) noexcept
{
    return part.enabled && !part.mute && (!anySolo || part.solo);
}

void PartMixer::mix(const PartRegistry& registry, const Inputs& inputs,
                    juce::AudioBuffer<float>& output,
                    const int requestedSamples) noexcept
{
    const int samples = juce::jlimit(0, output.getNumSamples(), requestedSamples);
    const bool anySolo = registry.anySolo();

    for (std::size_t i = 0; i < registry.size(); ++i) {
        const auto* input = inputs[i];
        const auto& part = registry[i];
        if (input == nullptr || !isAudible(part, anySolo))
            continue;

        const int count = juce::jmin(samples, input->getNumSamples());
        if (count <= 0 || input->getNumChannels() <= 0)
            continue;

        if (output.getNumChannels() == 1) {
            output.addFrom(0, 0, *input, 0, 0, count, part.level);
            continue;
        }

        const float leftGain = part.level * (part.pan > 0.0f ? 1.0f - part.pan : 1.0f);
        const float rightGain = part.level * (part.pan < 0.0f ? 1.0f + part.pan : 1.0f);
        output.addFrom(0, 0, *input, 0, 0, count, leftGain);
        const int rightSource = input->getNumChannels() > 1 ? 1 : 0;
        output.addFrom(1, 0, *input, rightSource, 0, count, rightGain);
    }
}

} // namespace vstengine::parts
