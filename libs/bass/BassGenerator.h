#pragma once

#include "sequence/Sequence.h"
#include <cstdint>
#include <string_view>

namespace vstengine::bass {
enum class BassProfile : std::uint8_t { psy, forest, dark };

class BassGenerator final {
public:
    [[nodiscard]] static sequence::Sequence generate(
        BassProfile, std::uint32_t globalSeed, std::uint64_t slotId,
        std::string_view instrumentId) noexcept;
};
} // namespace vstengine::bass
