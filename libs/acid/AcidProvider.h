#pragma once

#include "instrument/InstrumentContract.h"
#include <memory>

namespace vstengine::acid {
inline constexpr std::string_view instrumentId = "com.ultimavox.acid";
std::unique_ptr<instrument::InstrumentProvider> createAcidProvider();
} // namespace vstengine::acid
