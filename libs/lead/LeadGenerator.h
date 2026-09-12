#pragma once

#include "sequence/Sequence.h"
#include <cstdint>
#include <string_view>

namespace vstengine::lead {

enum class LeadProfile : std::uint8_t {
    psyArp,
    forestCall,
    alienPhrase,
    metallicSequence,
    hiTechBurst,
    hypnoticLead
};

class LeadGenerator final {
public:
    [[nodiscard]] static std::uint32_t stableSeed(
        std::uint32_t globalSeed, std::uint64_t slotId,
        std::string_view instrumentId) noexcept;
    [[nodiscard]] static sequence::Sequence generate(
        LeadProfile profile, std::uint32_t globalSeed, std::uint64_t slotId,
        std::string_view instrumentId = "com.ultimavox.lead") noexcept;
};

} // namespace vstengine::lead
