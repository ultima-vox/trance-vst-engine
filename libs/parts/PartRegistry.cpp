#include "parts/PartRegistry.h"
#include <cmath>

namespace vstengine::parts {

PartRegistry::PartRegistry()
    : parts_ {{
          { PartId::bass, "Bass", 1, true, false, false, 1.0f, 0.0f,
            false, EngineType::bass, vstengine::sequence::Sequence(16) },
          { PartId::kick, "Kick", 2, true, false, false, 1.0f, 0.0f,
            false, EngineType::kick, vstengine::sequence::Sequence(16) },
      }}
{
}

Part* PartRegistry::find(const PartId id) noexcept
{
    for (auto& part : parts_)
        if (part.id == id)
            return &part;
    return nullptr;
}

const Part* PartRegistry::find(const PartId id) const noexcept
{
    for (const auto& part : parts_)
        if (part.id == id)
            return &part;
    return nullptr;
}

bool PartRegistry::anySolo() const noexcept
{
    for (const auto& part : parts_)
        if (part.enabled && part.solo)
            return true;
    return false;
}

bool PartRegistry::isValid() const noexcept
{
    for (std::size_t i = 0; i < parts_.size(); ++i) {
        const auto& part = parts_[i];
        if (!parts::isValid(part.id) || !parts::isValid(part.engineType)
            || part.midiChannel < 1 || part.midiChannel > 16
            || !std::isfinite(part.level) || part.level < 0.0f
            || part.level > 2.0f || !std::isfinite(part.pan)
            || part.pan < -1.0f || part.pan > 1.0f
            || part.name.empty())
            return false;

        if ((part.id == PartId::bass) != (part.engineType == EngineType::bass))
            return false;
        for (std::size_t j = i + 1; j < parts_.size(); ++j)
            if (parts_[j].id == part.id)
                return false;
    }
    return true;
}

} // namespace vstengine::parts
