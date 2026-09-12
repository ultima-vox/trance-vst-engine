#pragma once

#include "instrument/InstrumentContract.h"
#include <memory>

namespace vstengine::atmos {
inline constexpr std::string_view instrumentId = "com.ultimavox.atmos-texture";
std::unique_ptr<instrument::InstrumentProvider> createAtmosProvider();
} // namespace vstengine::atmos
