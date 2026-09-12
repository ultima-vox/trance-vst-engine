#pragma once

#include "sequence/Sequence.h"
#include <cstdint>
#include <string_view>

namespace vstengine::atmos {

enum class AtmosProfile : std::uint8_t {
    deepSpace,
    forestBed,
    alienDrone,
    crystalAir,
    darkRitual,
    evolvingCloud
};

class AtmosGenerator final {
public:
    [[nodiscard]] static std::uint32_t stableSeed(
        std::uint32_t globalSeed, std::uint64_t slotId,
        std::string_view instrumentId) noexcept;
    [[nodiscard]] static sequence::Sequence generate(
        AtmosProfile profile, std::uint32_t globalSeed, std::uint64_t slotId,
        std::string_view instrumentId = "com.ultimavox.atmos-texture") noexcept;
};

} // namespace vstengine::atmos
