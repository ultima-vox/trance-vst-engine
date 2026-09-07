#include "part/Part.h"

namespace vstengine::part {

PartArray::PartArray()
{
    for (int i = 0; i < maxParts; ++i) {
        auto& part = parts_[static_cast<std::size_t>(i)];
        part.midiChannel = i + 1; // default 1:1; bass/kick overridden below
        part.sequence = vstengine::sequence::Sequence(16);
    }
    // Fixed multitimbral mapping per issue #11 §2.6.
    bass().engine = Engine::bass;
    bass().midiChannel = 1;
    kick().engine = Engine::kick;
    kick().midiChannel = 2;
}

} // namespace vstengine::part