#include "Sequence.h"
#include <random>

namespace vstengine::generator {

namespace {
    constexpr size_t lengthFromEnum(SequenceLength len) noexcept {
        switch (len) {
            case SequenceLength::steps16: return 16;
            case SequenceLength::steps32: return 32;
            case SequenceLength::steps64: return 64;
        }
        return 16;
    }

    SequenceLength enumFromLength(size_t len) noexcept {
        if (len <= 16) return SequenceLength::steps16;
        if (len <= 32) return SequenceLength::steps32;
        return SequenceLength::steps64;
    }
} // namespace

Sequence::Sequence()
    : length { 16 }
    , seqLength { SequenceLength::steps16 }
{
}

Sequence::Sequence(SequenceLength len)
    : length { lengthFromEnum(len) }
    , seqLength { len }
{
}

void Sequence::setLength(SequenceLength len) {
    seqLength = len;
    length = lengthFromEnum(len);
}

void Sequence::setTiming(TimingMode t) {
    timing = t;
}

void Sequence::clear() {
    for (size_t i = 0; i < length; ++i) {
        steps[i] = Step {};
    }
}

void Sequence::copyFrom(const Sequence& other, size_t destStart, size_t srcStart, size_t count) {
    const size_t n = std::min({ count, length - destStart, other.length - srcStart });
    for (size_t i = 0; i < n; ++i) {
        steps[destStart + i] = other.steps[srcStart + i];
    }
}

void Sequence::rotateLeft(size_t count) {
    if (count == 0 || length == 0) return;
    const size_t n = std::min(count, length);
    for (size_t i = 0; i < n; ++i) {
        const Step tmp = steps[0];
        for (size_t j = 0; j + 1 < length; ++j) {
            steps[j] = steps[j + 1];
        }
        steps[length - 1] = tmp;
    }
}

void Sequence::rotateRight(size_t count) {
    if (count == 0 || length == 0) return;
    const size_t n = std::min(count, length);
    for (size_t i = 0; i < n; ++i) {
        const Step tmp = steps[length - 1];
        for (size_t j = length; j > 0; --j) {
            steps[j] = steps[j - 1];
        }
        steps[0] = tmp;
    }
}

void Sequence::reverse() {
    for (size_t i = 0, j = length; i < j / 2; ++i, --j) {
        std::swap(steps[i], steps[j - 1]);
    }
}

void Sequence::transpose(int semitones) {
    for (size_t i = 0; i < length; ++i) {
        steps[i].noteOffset = std::clamp(steps[i].noteOffset + semitones, -48, 48);
    }
}

void Sequence::octaveUp() {
    transpose(12);
}

void Sequence::octaveDown() {
    transpose(-12);
}

void Sequence::shiftLeft(size_t count) {
    if (count == 0 || length == 0) return;
    const size_t n = std::min(count, length);
    for (size_t i = 0; i < n; ++i) {
        const Step tmp = steps[0];
        for (size_t j = 0; j + 1 < length; ++j) {
            steps[j] = steps[j + 1];
        }
        steps[length - 1] = tmp;
    }
}

void Sequence::shiftRight(size_t count) {
    if (count == 0 || length == 0) return;
    const size_t n = std::min(count, length);
    for (size_t i = 0; i < n; ++i) {
        const Step tmp = steps[length - 1];
        for (size_t j = length; j > 0; --j) {
            steps[j] = steps[j - 1];
        }
        steps[0] = tmp;
    }
}

void Sequence::mutateSelected(const std::vector<size_t>& indices, uint32_t seed) {
    std::mt19937 rng { seed };

    auto pick = [&rng](float lo, float hi) {
        std::uniform_int_distribution<int> d(static_cast<int>(lo * 100), static_cast<int>(hi * 100));
        return static_cast<float>(d(rng)) / 100.0f;
    };

    auto pickInt = [&rng](int lo, int hi) {
        std::uniform_int_distribution<int> d(lo, hi);
        return d(rng);
    };

    for (size_t idx : indices) {
        if (idx >= length) continue;
        auto& step = steps[idx];
        const float r = pick(0.0f, 1.0f);

        if (r < 0.30f) {
            step.gate = !step.gate;
        } else if (r < 0.50f) {
            const int dir = pickInt(0, 1) == 0 ? -1 : 1;
            const int amount = pickInt(1, 12);
            step.noteOffset = std::clamp(step.noteOffset + dir * amount, -48, 48);
        } else if (r < 0.70f) {
            step.velocity = pick(0.1f, 1.0f);
        } else if (r < 0.80f) {
            step.accent = pick(0.0f, 1.0f);
        }
    }
}

void Sequence::clearSelected(const std::vector<size_t>& indices) {
    for (size_t idx : indices) {
        if (idx < length) {
            steps[idx].gate = false;
        }
    }
}

void Sequence::regenerateBySeed(uint32_t) {
    clear();
}

void Sequence::serialize(juce::MemoryBlock& dest) const {
    dest.clear();

    int32_t tmp;

    tmp = static_cast<int32_t>(seqLength);
    dest.write(&tmp, sizeof(tmp));

    tmp = static_cast<int32_t>(timing);
    dest.write(&tmp, sizeof(tmp));

    tmp = static_cast<int32_t>(length);
    dest.write(&tmp, sizeof(tmp));

    for (size_t i = 0; i < length; ++i) {
        const auto& s = steps[i];

        uint8_t gateByte = s.gate ? 1 : 0;
        dest.write(&gateByte, 1);

        tmp = static_cast<int32_t>(s.noteOffset);
        dest.write(&tmp, sizeof(tmp));

        dest.write(&s.velocity, sizeof(s.velocity));
        dest.write(&s.accent, sizeof(s.accent));
        dest.write(&s.probability, sizeof(s.probability));

        tmp = static_cast<int32_t>(s.ratchetCount);
        dest.write(&tmp, sizeof(tmp));

        tmp = static_cast<int32_t>(s.slideDuration);
        dest.write(&tmp, sizeof(tmp));

        dest.write(&s.gateWidth, sizeof(s.gateWidth));
    }
}

Sequence Sequence::deserialize(const juce::MemoryBlock& src) {
    Sequence seq;

    if (src.getSize() < 8) return seq;

    int32_t tmp;
    size_t pos = 0;

    // Read seqLength
    src.copyInto(&tmp, pos, sizeof(tmp));
    pos += sizeof(tmp);
    if (tmp >= 0 && tmp <= static_cast<int32_t>(SequenceLength::steps64)) {
        seq.seqLength = static_cast<SequenceLength>(tmp);
        seq.length = lengthFromEnum(seq.seqLength);
    }

    // Read timing
    src.copyInto(&tmp, pos, sizeof(tmp));
    pos += sizeof(tmp);
    if (tmp >= 0 && tmp <= static_cast<int32_t>(TimingMode::triplet)) {
        seq.timing = static_cast<TimingMode>(tmp);
    }

    // Read length (validate)
    src.copyInto(&tmp, pos, sizeof(tmp));
    pos += sizeof(tmp);
    if (tmp > 0 && tmp <= static_cast<int32_t>(Sequence::maxSteps)) {
        seq.length = static_cast<size_t>(tmp);
    }

    const size_t stepSize = 29;
    const size_t expectedSize = 12 + seq.length * stepSize;

    if (src.getSize() < expectedSize) return seq;

    for (size_t i = 0; i < seq.length; ++i) {
        auto& s = seq.steps[i];

        uint8_t gateByte;
        src.copyInto(&gateByte, pos, 1);
        s.gate = gateByte != 0;
        pos += 1;

        src.copyInto(&tmp, pos, sizeof(tmp));
        s.noteOffset = tmp;
        pos += sizeof(tmp);

        src.copyInto(&s.velocity, pos, sizeof(s.velocity));
        pos += sizeof(s.velocity);
        src.copyInto(&s.accent, pos, sizeof(s.accent));
        pos += sizeof(s.accent);
        src.copyInto(&s.probability, pos, sizeof(s.probability));
        pos += sizeof(s.probability);

        src.copyInto(&tmp, pos, sizeof(tmp));
        s.ratchetCount = tmp;
        pos += sizeof(tmp);

        src.copyInto(&tmp, pos, sizeof(tmp));
        s.slideDuration = tmp;
        pos += sizeof(tmp);

        src.copyInto(&s.gateWidth, pos, sizeof(s.gateWidth));
        pos += sizeof(s.gateWidth);
    }

    return seq;
}

} // namespace vstengine::generator
