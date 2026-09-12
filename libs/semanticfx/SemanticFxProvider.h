#pragma once

#include "instrument/InstrumentContract.h"
#include <memory>

namespace vstengine::semanticfx {

inline constexpr std::string_view instrumentId =
    "com.ultimavox.semantic-fx";

std::unique_ptr<instrument::InstrumentProvider> createSemanticFxProvider();

} // namespace vstengine::semanticfx
