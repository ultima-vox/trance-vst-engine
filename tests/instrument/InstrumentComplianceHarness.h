#pragma once

#include "instrument/InstrumentContract.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vstengine::tests {

struct InstrumentComplianceOptions {
    std::uint8_t testNote { 60 };
    std::uint8_t testVelocity { 100 };
    bool requireAudibleNote { true };
    double silenceEnergy { 1.0e-12 };
    double audibleEnergy { 1.0e-10 };
};

struct InstrumentComplianceReport {
    std::uint32_t checks {};
    std::vector<std::string> failures;

    [[nodiscard]] bool passed() const noexcept { return failures.empty(); }
    explicit operator bool() const noexcept { return passed(); }
};

// Test-only common contract suite. Caller owns provider for entire call.
// No test framework dependency: executable may print failures using its own
// assertion/reporting convention.
[[nodiscard]] InstrumentComplianceReport runInstrumentCompliance(
    instrument::InstrumentProvider& provider,
    std::string_view instrumentId,
    const InstrumentComplianceOptions& options = {});

} // namespace vstengine::tests
