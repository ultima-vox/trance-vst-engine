#pragma once

#include "sequence/Sequence.h"
#include <cstdint>
#include <string_view>

namespace vstengine::semanticfx {

enum class SemanticFxProfile : std::uint8_t {
    sweep,
    laser,
    riser,
    downlifter,
    impact,
    whoosh,
    zap,
    noiseBurst,
    metallic,
    alien
};

class SemanticFxGenerator final {
public:
    [[nodiscard]] static sequence::Sequence generate(
        SemanticFxProfile profile, std::uint32_t globalSeed,
        std::uint64_t slotId,
        std::string_view instrumentId = "com.ultimavox.semantic-fx") noexcept;
};

} // namespace vstengine::semanticfx
