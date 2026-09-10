#pragma once

#include "parts/Part.h"
#include <array>
#include <cstddef>

namespace vstengine::parts {

class PartRegistry final {
public:
    static constexpr std::size_t capacity = 2;

    PartRegistry();

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return parts_.size();
    }

    [[nodiscard]] Part& operator[](std::size_t index) noexcept
    {
        return parts_[index];
    }

    [[nodiscard]] const Part& operator[](std::size_t index) const noexcept
    {
        return parts_[index];
    }

    [[nodiscard]] Part* find(PartId id) noexcept;
    [[nodiscard]] const Part* find(PartId id) const noexcept;
    [[nodiscard]] bool anySolo() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;

private:
    std::array<Part, capacity> parts_;
};

} // namespace vstengine::parts
