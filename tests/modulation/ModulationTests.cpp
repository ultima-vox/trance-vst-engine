#include "modulation/Modulation.h"
#include "modulation/ModulationState.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace {
std::atomic<bool> trackAllocations { false };
std::atomic<std::size_t> allocationCount { 0 };
}

void* operator new(std::size_t size)
{
    if (trackAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (auto* memory = std::malloc(size)) return memory;
    throw std::bad_alloc {};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
int run = 0;
int passed = 0;
#define REQUIRE(condition, message)                                      \
    do {                                                                  \
        ++run;                                                            \
        if (!(condition)) {                                               \
            std::cerr << "FAILED: " << message << " line " << __LINE__   \
                      << '\n';                                            \
            std::exit(EXIT_FAILURE);                                      \
        }                                                                 \
        ++passed;                                                         \
    } while (false)

vstengine::instrument::ParameterDescriptor parameter(
    std::string id, float minimum, float maximum, float defaultValue,
    float step = 0.0f, bool modulatable = true)
{
    vstengine::instrument::ParameterDescriptor result;
    result.id = std::move(id);
    result.name = result.id;
    result.minimum = minimum;
    result.maximum = maximum;
    result.defaultValue = defaultValue;
    result.step = step;
    result.modulatable = modulatable;
    return result;
}

vstengine::instrument::InstrumentDescriptor descriptor(
    std::string id, std::vector<vstengine::instrument::ParameterDescriptor> params)
{
    vstengine::instrument::InstrumentDescriptor result;
    result.id = std::move(id);
    result.parameters = std::move(params);
    result.capabilities |= vstengine::instrument::Capability::modulation;
    result.budget.maxModulationRoutes = 8;
    return result;
}

} // namespace

int main()
{
    using namespace vstengine;
    using namespace modulation;

    const auto bass = descriptor("com.ultimavox.bass", {
        parameter("cutoff", 20.0f, 20000.0f, 1000.0f),
        parameter("resonance", 0.0f, 1.0f, 0.2f),
        parameter("internal", 0.0f, 1.0f, 0.0f, 0.0f, false)
    });
    const auto acid = descriptor("com.ultimavox.acid", {
        parameter("cutoff", 40.0f, 16000.0f, 800.0f),
        parameter("wave", 0.0f, 3.0f, 0.0f, 1.0f)
    });

    DestinationRegistry registry;
    REQUIRE(registry.add(bass) == RegistryStatus::ok,
            "Bass descriptor registers");
    REQUIRE(registry.add(acid) == RegistryStatus::ok,
            "Acid descriptor registers");
    REQUIRE(registry.destinations().size() == 4,
            "only modulatable parameters become destinations");
    REQUIRE(registry.find("com.ultimavox.bass", "internal") == nullptr,
            "non-modulatable parameter excluded");
    REQUIRE(registry.find("com.ultimavox.bass", "cutoff")->key
                != registry.find("com.ultimavox.acid", "cutoff")->key,
            "InstrumentId participates in stable identity");
    REQUIRE(destinationKey("com.ultimavox.bass", "cutoff")
                == destinationKey("com.ultimavox.bass", "cutoff"),
            "destination key deterministic");
    REQUIRE(registry.add(bass) == RegistryStatus::duplicateParameter,
            "duplicate descriptor rejected");

    auto malformed = descriptor("com.ultimavox.bad", {
        parameter("good", 0.0f, 1.0f, 0.5f),
        parameter("bad", 1.0f, 0.0f, 0.5f)
    });
    const auto countBeforeBad = registry.destinations().size();
    REQUIRE(registry.add(malformed) == RegistryStatus::invalidDescriptor,
            "malformed destination rejected");
    REQUIRE(registry.destinations().size() == countBeforeBad,
            "registry update transactional");

    ModulationMatrix matrix;
    REQUIRE(matrix.prepare(registry, 5) == MatrixStatus::ok,
            "declared route budget accepted");
    REQUIRE(matrix.addRoute({ { SourceKind::macro, 0 },
                              SourceTransform::direct,
                              destinationKey(bass.id, "cutoff"), 0.5f })
                == MatrixStatus::ok,
            "macro route accepted");
    REQUIRE(matrix.addRoute({ { SourceKind::lfo, 1 },
                              SourceTransform::direct,
                              destinationKey(bass.id, "cutoff"), -0.25f })
                == MatrixStatus::ok,
            "LFO route accepted");
    REQUIRE(matrix.addRoute({ { SourceKind::velocity, 0 },
                              SourceTransform::bipolar,
                              destinationKey(acid.id, "wave"), 0.5f })
                == MatrixStatus::ok,
            "velocity route accepted");
    REQUIRE(matrix.addRoute({ { SourceKind::modWheel, 0 },
                              SourceTransform::direct,
                              destinationKey(acid.id, "cutoff"), 0.1f })
                == MatrixStatus::ok,
            "mod wheel route accepted");
    REQUIRE(matrix.addRoute({ { SourceKind::aftertouch, 0 },
                              SourceTransform::direct,
                              destinationKey(bass.id, "resonance"), 0.2f })
                == MatrixStatus::ok,
            "aftertouch route accepted");
    REQUIRE(matrix.addRoute({ { SourceKind::macro, 2 },
                              SourceTransform::direct,
                              destinationKey(acid.id, "cutoff"), 0.1f })
                == MatrixStatus::routeBudgetExceeded,
            "route budget rejects overflow");

    SourceValues sources;
    sources.macros[0] = 0.8f;
    sources.lfos[1] = -0.4f;
    sources.velocity = 0.25f;
    sources.modWheel = 0.5f;
    sources.aftertouch = 0.75f;
    std::array<float, maxDestinations> bases {};
    bases[registry.indexOf(destinationKey(bass.id, "cutoff"))] = 0.2f;
    bases[registry.indexOf(destinationKey(bass.id, "resonance"))] = 0.1f;
    bases[registry.indexOf(destinationKey(acid.id, "cutoff"))] = 0.4f;
    bases[registry.indexOf(destinationKey(acid.id, "wave"))] = 0.5f;

    allocationCount = 0;
    trackAllocations = true;
    const auto values = matrix.evaluate(sources, bases);
    trackAllocations = false;
    REQUIRE(allocationCount == 0, "realtime evaluation allocates nothing");
    REQUIRE(values.size() == 4, "one result per routed destination");
    REQUIRE(std::abs(values[0].normalized - 0.7f) < 1.0e-6f,
            "macro and LFO sum in normalized domain");
    REQUIRE(std::abs(values[1].normalized - 0.25f) < 1.0e-6f,
            "aftertouch source evaluates");
    REQUIRE(std::abs(values[2].normalized - 0.45f) < 1.0e-6f,
            "mod wheel source evaluates");
    REQUIRE(std::abs(values[3].normalized - 0.25f) < 1.0e-6f,
            "bipolar velocity evaluates");
    REQUIRE(values[3].plain == 1.0f, "stepped destination quantizes");

    const auto firstPass = std::array {
        values[0].normalized, values[1].normalized,
        values[2].normalized, values[3].normalized
    };
    const auto repeat = matrix.evaluate(sources, bases);
    REQUIRE(repeat.size() == firstPass.size(), "repeat result shape stable");
    for (std::size_t i = 0; i < repeat.size(); ++i)
        REQUIRE(repeat[i].normalized == firstPass[i],
                "evaluation bit-deterministic");

    sources.macros[0] = std::numeric_limits<float>::quiet_NaN();
    const auto finite = matrix.evaluate(sources, bases);
    for (const auto& value : finite)
        REQUIRE(std::isfinite(value.normalized) && std::isfinite(value.plain),
                "non-finite source cannot escape runtime");

    PersistentState captured;
    REQUIRE(captureState(matrix, registry, captured) == StateStatus::ok,
            "persistent routes captured without runtime values");
    std::array<std::byte, 4096> encoded {};
    std::size_t written {};
    REQUIRE(serializeState(captured, encoded, written) == StateStatus::ok,
            "state serialized into caller buffer");
    REQUIRE(written == serializedSize(captured), "serialized size exact");
    PersistentState decoded;
    REQUIRE(deserializeState({ encoded.data(), written }, decoded)
                == StateStatus::ok,
            "state decoded");
    std::array<std::byte, 4096> canonical {};
    std::size_t canonicalSize {};
    REQUIRE(serializeState(decoded, canonical, canonicalSize) == StateStatus::ok
                && canonicalSize == written
                && std::equal(encoded.begin(), encoded.begin() + written,
                              canonical.begin()),
            "save-load-save bytes canonical");

    ModulationMatrix restored;
    REQUIRE(restoreState(decoded, registry, 5, restored) == StateStatus::ok,
            "valid state restores");
    const auto restoredValues = restored.evaluate(sources, bases);
    REQUIRE(restoredValues.size() == finite.size(),
            "restored route graph equivalent");

    const auto previousRouteCount = restored.routes().size();
    auto missing = decoded;
    missing.routes[0].instrumentId[0] = 'x';
    REQUIRE(restoreState(missing, registry, 5, restored)
                == StateStatus::destinationMissing,
            "missing destination remains explicit");
    REQUIRE(restored.routes().size() == previousRouteCount,
            "failed restore leaves live graph unchanged");
    REQUIRE(restoreState(decoded, registry, 4, restored)
                == StateStatus::routeBudgetExceeded,
            "stored graph cannot exceed declared budget");

    auto corruptBytes = encoded;
    corruptBytes[0] = std::byte { 0 };
    REQUIRE(deserializeState({ corruptBytes.data(), written }, decoded)
                == StateStatus::corrupt,
            "corrupt magic rejected");
    REQUIRE(deserializeState({ encoded.data(), written - 1 }, decoded)
                == StateStatus::corrupt,
            "truncated state rejected");
    auto nonCanonicalReservedByte = encoded;
    nonCanonicalReservedByte[11] = std::byte { 1 };
    REQUIRE(deserializeState({ nonCanonicalReservedByte.data(), written }, decoded)
                == StateStatus::corrupt,
            "non-zero route reserved byte rejected");
    auto nonCanonicalReservedWord = encoded;
    nonCanonicalReservedWord[18] = std::byte { 1 };
    REQUIRE(deserializeState({ nonCanonicalReservedWord.data(), written }, decoded)
                == StateStatus::corrupt,
            "non-zero route reserved word rejected");
    REQUIRE(matrix.addRoute({ { SourceKind::lfo,
                                static_cast<std::uint8_t>(maxLfos) },
                              SourceTransform::direct,
                              destinationKey(acid.id, "cutoff"), 0.1f })
                == MatrixStatus::routeBudgetExceeded,
            "budget rejection deterministic before route validation");

    ModulationMatrix validation;
    REQUIRE(validation.prepare(registry, 8) == MatrixStatus::ok,
            "validation matrix prepared");
    REQUIRE(validation.addRoute({ { SourceKind::lfo,
                                    static_cast<std::uint8_t>(maxLfos) },
                                  SourceTransform::direct,
                                  destinationKey(acid.id, "cutoff"), 0.1f })
                == MatrixStatus::invalidSource,
            "invalid LFO index rejected");
    REQUIRE(validation.addRoute({ { SourceKind::macro, 0 },
                                  SourceTransform::direct, 1234, 0.1f })
                == MatrixStatus::invalidDestination,
            "unresolved destination rejected without fallback");
    for (const auto source : { SourceKind::envelope, SourceKind::stepMod,
                               SourceKind::sampleAndHold,
                               SourceKind::random })
        REQUIRE(validation.addRoute({ { source, 0 }, SourceTransform::direct,
                    destinationKey(acid.id, "cutoff"), 0.1f })
                    == MatrixStatus::ok,
                "required modulation source accepted");
    SourceValues requiredSources;
    requiredSources.envelope = 0.2f;
    requiredSources.stepMod = 0.4f;
    requiredSources.sampleAndHold = 0.6f;
    requiredSources.random = 0.8f;
    REQUIRE(validation.evaluate(requiredSources, bases).size() == 1,
            "required sources evaluate into real destination");
    ModulationMatrix disabled;
    REQUIRE(disabled.prepare(registry, 0) == MatrixStatus::ok,
            "zero-route budget represents disabled modulation");
    REQUIRE(disabled.addRoute({ { SourceKind::macro, 0 },
                                SourceTransform::direct,
                                destinationKey(bass.id, "cutoff"), 0.1f })
                == MatrixStatus::routeBudgetExceeded,
            "zero-route budget rejects routes");

    std::cout << "Modulation tests passed (" << passed << "/" << run << ")\n";
    return EXIT_SUCCESS;
}
