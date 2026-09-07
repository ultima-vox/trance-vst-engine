#pragma once
#include "sequence/Sequence.h"
#include <array>
#include <cstdint>

namespace vstengine::part {

// Engine type per Part (issue #11 §2.6 fixed multitimbral mapping).
enum class Engine : uint8_t {
    bass,
    kick,
};

// A single multitimbral Part (issue #11 PHASE 7).
//
// Each Part owns its own engine type, MIDI channel, sequence and mixing
// state. Parts 7-16 are reserved until they are actually assignable; the
// array still stores them so project state is stable.
struct Part {
    Engine engine { Engine::bass };
    int midiChannel { 1 };
    vstengine::sequence::Sequence sequence { 16 };

    bool mute { false };
    bool solo { false };
    bool lock { false };
    float level { 1.0f }; // 0..2 linear gain
    float pan { 0.0f };   // -1 (L) .. +1 (R)
};

static constexpr int maxParts = 16;

// Fixed-capacity collection of Parts (issue #11 PHASE 7).
//
// Indices are stable: 0 = Bass (CH 1), 1 = Kick (CH 2), 2..15 = reserved.
class PartArray {
public:
    PartArray();

    [[nodiscard]] int size() const noexcept { return maxParts; }

    [[nodiscard]] Part& operator[](int index) noexcept
    {
        return parts_[static_cast<std::size_t>(
            index >= 0 && index < maxParts ? index : 0)];
    }
    [[nodiscard]] const Part& operator[](int index) const noexcept
    {
        return parts_[static_cast<std::size_t>(
            index >= 0 && index < maxParts ? index : 0)];
    }

    // Canonical part indices.
    static constexpr int bassIndex = 0;
    static constexpr int kickIndex = 1;

    [[nodiscard]] Part& bass() noexcept { return (*this)[bassIndex]; }
    [[nodiscard]] Part& kick() noexcept { return (*this)[kickIndex]; }
    [[nodiscard]] const Part& bass() const noexcept
    {
        return (*this)[bassIndex];
    }
    [[nodiscard]] const Part& kick() const noexcept
    {
        return (*this)[kickIndex];
    }

    // True when any Part has solo engaged (mixing rule).
    [[nodiscard]] bool anySolo() const noexcept
    {
        for (const auto& part : parts_)
            if (part.solo)
                return true;
        return false;
    }

private:
    std::array<Part, maxParts> parts_ {};
};

} // namespace vstengine::part