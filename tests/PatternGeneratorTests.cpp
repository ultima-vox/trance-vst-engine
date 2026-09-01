#include "generator/PatternGenerator.h"
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace vstengine::generator;
    const auto a = PatternGenerator::generate(Style::darkPsy, 12345u);
    const auto b = PatternGenerator::generate(Style::darkPsy, 12345u);

    for (std::size_t i = 0; i < a.size(); ++i) {
        require(a[i].gate == b[i].gate, "same seed must reproduce gates");
        require(a[i].accent == b[i].accent, "same seed must reproduce accents");
        require(a[i].noteOffset == b[i].noteOffset, "same seed must reproduce notes");
        if (i % 4 == 0)
            require(!a[i].gate, "quarter-note downbeat must remain free for kick");
    }

    require(a[1].gate && a[2].gate && a[3].gate, "rolling bass core must be present");
    std::cout << "PatternGenerator tests passed\n";
    return EXIT_SUCCESS;
}
