#pragma once

#include "instrument/InstrumentAbi.h"
#include "sequence/Sequence.h"
#include <cstddef>
#include <span>
#include <vector>

namespace vstengine::sequence {
inline constexpr std::size_t maximumEncodedPatternBytes =
    16u + VOX_PATTERN_MAX_STEPS * 24u;

[[nodiscard]] bool validPattern(const VoxPatternV1&) noexcept;
[[nodiscard]] VoxPatternV1 toPattern(const Sequence&) noexcept;
[[nodiscard]] bool fromPattern(const VoxPatternV1&, Sequence&) noexcept;
[[nodiscard]] bool encodePattern(const VoxPatternV1&,
                                 std::vector<std::byte>& destination);
[[nodiscard]] bool decodePattern(std::span<const std::byte>,
                                 VoxPatternV1&) noexcept;
[[nodiscard]] std::uint32_t patternFingerprint(
    const VoxPatternV1&) noexcept;
[[nodiscard]] bool mergePatternMutation(
    const VoxPatternV1& current, const VoxPatternV1& generated,
    float amount, int selectedStart, int selectedEnd,
    VoxPatternV1& destination) noexcept;
} // namespace vstengine::sequence
