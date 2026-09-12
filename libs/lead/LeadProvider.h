#pragma once

#include "instrument/InstrumentContract.h"
#include <memory>

namespace vstengine::lead {
inline constexpr std::string_view instrumentId = "com.ultimavox.lead";
std::unique_ptr<instrument::InstrumentProvider> createLeadProvider();
} // namespace vstengine::lead
