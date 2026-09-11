#include "instrument/HostParameterSchema.h"
#include <cstdio>
#include <stdexcept>

namespace vstengine::instrument::hostparams {
namespace {
void validate(std::size_t slotIndex, std::size_t macroIndex)
{
    if (slotIndex >= maxSlots || macroIndex >= macrosPerSlot)
        throw std::out_of_range("slot macro index");
}
} // namespace

std::string macroId(std::size_t slotIndex, std::size_t macroIndex)
{
    validate(slotIndex, macroIndex);
    std::array<char, 32> value {};
    std::snprintf(value.data(), value.size(), "slot%02zuMacro%02zu",
                  slotIndex + 1, macroIndex + 1);
    return value.data();
}

std::string macroName(std::size_t slotIndex, std::size_t macroIndex)
{
    validate(slotIndex, macroIndex);
    return "Slot " + std::to_string(slotIndex + 1) + " "
         + std::string(macroLabels[macroIndex]);
}
} // namespace vstengine::instrument::hostparams
