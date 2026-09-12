#include "instrument/InstrumentComplianceHarness.h"
#include "modules/BuiltInProvider.h"
#include <cstdlib>
#include <iostream>

int main()
{
    auto provider = vstengine::modules::createBuiltInProvider();
    for (const auto id : { vstengine::modules::bassInstrumentId,
                           vstengine::modules::acidInstrumentId,
                           vstengine::modules::leadInstrumentId,
                           vstengine::modules::semanticFxInstrumentId,
                           vstengine::modules::atmosInstrumentId }) {
        const auto report = vstengine::tests::runInstrumentCompliance(
            *provider, id);
        if (!report) {
            for (const auto& failure : report.failures)
                std::cerr << id << ": " << failure << '\n';
            return EXIT_FAILURE;
        }
        std::cout << id << ": " << report.checks << " checks\n";
    }
    return EXIT_SUCCESS;
}
