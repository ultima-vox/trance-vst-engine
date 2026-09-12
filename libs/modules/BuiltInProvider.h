#pragma once
#include "instrument/InstrumentContract.h"
#include <memory>

namespace vstengine::modules {
std::unique_ptr<instrument::InstrumentProvider> createBuiltInProvider();
inline constexpr std::string_view bassInstrumentId = "com.ultimavox.psy-bass";
inline constexpr std::string_view acidInstrumentId = "com.ultimavox.acid";
} // namespace vstengine::modules
