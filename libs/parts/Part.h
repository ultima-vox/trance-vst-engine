#pragma once

#include "sequence/Sequence.h"
#include <cstdint>
#include <string_view>

namespace vstengine::parts {

enum class PartId : std::uint8_t {
    bass = 1,
    kick = 2,
};

enum class EngineType : std::uint8_t {
    bass = 1,
    kick = 2,
};

struct Part final {
    PartId id { PartId::bass };
    std::string_view name { "Bass" };
    int midiChannel { 1 };
    bool enabled { true };
    bool mute { false };
    bool solo { false };
    float level { 1.0f };
    float pan { 0.0f };
    bool locked { false };
    EngineType engineType { EngineType::bass };
    vstengine::sequence::Sequence sequence { 16 };
};

[[nodiscard]] constexpr bool isValid(PartId id) noexcept
{
    return id == PartId::bass || id == PartId::kick;
}

[[nodiscard]] constexpr bool isValid(EngineType type) noexcept
{
    return type == EngineType::bass || type == EngineType::kick;
}

} // namespace vstengine::parts
