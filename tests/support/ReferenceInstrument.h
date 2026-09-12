#pragma once

#include "instrument/InstrumentContract.h"
#include <memory>
#include <string_view>

namespace vstengine::tests {

inline constexpr std::string_view referenceInstrumentId =
    "com.ultimavox.test.reference-instrument";

// Test-only provider. Never link this target into Vox Electronic Engine.
[[nodiscard]] std::unique_ptr<instrument::InstrumentProvider>
createReferenceInstrumentProvider();

} // namespace vstengine::tests
