#pragma once

#include "sequence/Sequence.h"
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>

namespace vstengine::sequence {

// Single-writer message-thread publication into audio-thread snapshot.
// All shared fields are lock-free atomics. Audio attempts one bounded read per
// block; if publication overlaps, previous complete snapshot remains active.
class RealtimeSequenceBridge final {
public:
    RealtimeSequenceBridge()
    {
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
        static_assert(std::atomic<int>::is_always_lock_free);
    }

    void publish(const Sequence& source) noexcept
    {
        generation.fetch_add(1, std::memory_order_acq_rel); // odd = writing
        length.store(source.size(), std::memory_order_relaxed);
        timing.store(static_cast<int>(source.getTimingMode()),
                     std::memory_order_relaxed);
        for (int i = 0; i < Sequence::maxSteps; ++i) {
            const auto& value = source[i];
            auto& target = steps[static_cast<std::size_t>(i)];
            target.gate.store(value.gate, std::memory_order_relaxed);
            target.noteOffset.store(value.noteOffset, std::memory_order_relaxed);
            target.velocity.store(bits(value.velocity), std::memory_order_relaxed);
            target.accent.store(value.accent, std::memory_order_relaxed);
            target.probability.store(bits(value.probability), std::memory_order_relaxed);
            target.ratchet.store(value.ratchetCount, std::memory_order_relaxed);
            target.slide.store(bits(value.slideDuration), std::memory_order_relaxed);
            target.gateWidth.store(bits(value.gateWidth), std::memory_order_relaxed);
        }
        generation.fetch_add(1, std::memory_order_release); // even = complete
    }

    [[nodiscard]] bool read(Sequence& destination) const noexcept
    {
        const auto before = generation.load(std::memory_order_acquire);
        if ((before & 1u) != 0u)
            return false;

        Sequence candidate(length.load(std::memory_order_relaxed));
        candidate.setTimingMode(static_cast<TimingMode>(juce::jlimit(
            0, static_cast<int>(TimingMode::triplet),
            timing.load(std::memory_order_relaxed))));
        for (int i = 0; i < Sequence::maxSteps; ++i) {
            const auto& source = steps[static_cast<std::size_t>(i)];
            auto& value = candidate[i];
            value.gate = source.gate.load(std::memory_order_relaxed) != 0;
            value.noteOffset = source.noteOffset.load(std::memory_order_relaxed);
            value.velocity = number(source.velocity.load(std::memory_order_relaxed));
            value.accent = source.accent.load(std::memory_order_relaxed) != 0;
            value.probability = number(source.probability.load(std::memory_order_relaxed));
            value.ratchetCount = source.ratchet.load(std::memory_order_relaxed);
            value.slideDuration = number(source.slide.load(std::memory_order_relaxed));
            value.gateWidth = number(source.gateWidth.load(std::memory_order_relaxed));
        }

        const auto after = generation.load(std::memory_order_acquire);
        if (before != after || (after & 1u) != 0u)
            return false;
        destination = candidate;
        return true;
    }

private:
    struct AtomicStep {
        std::atomic<int> gate {};
        std::atomic<int> noteOffset {};
        std::atomic<std::uint32_t> velocity { bits(1.0f) };
        std::atomic<int> accent {};
        std::atomic<std::uint32_t> probability { bits(1.0f) };
        std::atomic<int> ratchet { 1 };
        std::atomic<std::uint32_t> slide {};
        std::atomic<std::uint32_t> gateWidth { bits(0.75f) };
    };

    static constexpr std::uint32_t bits(float value) noexcept
    {
        return std::bit_cast<std::uint32_t>(value);
    }
    static constexpr float number(std::uint32_t value) noexcept
    {
        return std::bit_cast<float>(value);
    }

    std::atomic<std::uint32_t> generation {};
    std::atomic<int> length { 16 };
    std::atomic<int> timing { static_cast<int>(TimingMode::sixteenth) };
    std::array<AtomicStep, Sequence::maxSteps> steps {};
};

} // namespace vstengine::sequence
