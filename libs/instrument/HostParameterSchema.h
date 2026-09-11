#pragma once
#include "instrument/InstrumentContract.h"
#include <array>
#include <string>
#include <string_view>

namespace vstengine::instrument::hostparams {

inline constexpr std::array<std::string_view, macrosPerSlot> macroLabels {
    "Cutoff", "Resonance", "Envelope", "Decay",
    "Drive", "Accent", "Space", "Motion"
};

std::string macroId(std::size_t slotIndex, std::size_t macroIndex);
std::string macroName(std::size_t slotIndex, std::size_t macroIndex);

} // namespace vstengine::instrument::hostparams
