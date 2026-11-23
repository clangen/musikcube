#include "../../support/check.h"

#include <musikcore/audio/PlaybackMath.h>
#include <musikcore/sdk/DataBuffer.h>
#include <plugins/portaudioout/PcmFrames.h>

#include <array>
#include <limits>
#include <new>

using namespace musik::core::audio;

namespace {
    /* force a growth failure without exhausting the machine's memory. */
    struct FailingElement {
        static inline bool failNext = false;
        int value = 0;

        FailingElement() {
            if (failNext) {
                failNext = false;
                throw std::bad_alloc();
            }
        }
    };

    bool preservesBufferOnFailedGrowth() {
        DataBuffer<FailingElement> buffer;
        buffer.reset(1);
        buffer.pos()->value = 42;
        FailingElement::failNext = true;
        try {
            buffer.reset(2);
        }
        catch (const std::bad_alloc&) {
            return buffer.avail() == 1 && buffer.pos()->value == 42;
        }
        return false;
    }
}

int main() {
    if (!TEST_CHECK(preservesBufferOnFailedGrowth())) {
        return 1;
    }
    DataBuffer<char> first;
    DataBuffer<char> second;
    first.append("abcd", 4);
    second.append("xyz", 3);
    first.inc(2);
    second.inc(1);
    first.swap(second);
    if (!TEST_CHECK(first.avail() == 2 && *first.pos() == 'y') ||
        !TEST_CHECK(second.avail() == 2 && *second.pos() == 'c'))
    {
        return 1;
    }

    for (const std::size_t channels : { 1, 2, 6 }) {
        std::array<float, 24> source {};
        std::array<float, 26> target;
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] = static_cast<float>(i + 1);
        }
        target.fill(-1.0f);

        copyPcmFrames(target.data() + 1, source.data(), 1, 2, channels);
        silencePcmFrames(target.data() + 1 + 2 * channels, 1, channels);
        for (std::size_t i = 0; i < 2 * channels; ++i) {
            if (!TEST_CHECK(target[i + 1] == source[i + channels])) {
                return 1;
            }
        }
        for (std::size_t i = 0; i < channels; ++i) {
            if (!TEST_CHECK(target[1 + 2 * channels + i] == 0.0f)) {
                return 1;
            }
        }
        if (!TEST_CHECK(target[0] == -1.0f && target[1 + 3 * channels] == -1.0f)) {
            return 1;
        }
    }

    return TEST_CHECK(pcmBufferCount(5.0, 48000, 2048, 2) == 118) &&
        TEST_CHECK(pcmBufferCount(0.0, 48000, 2048, 2) == 2) &&
        TEST_CHECK(playbackGain(1.1f, 2.0f, true) == 1.1f) &&
        TEST_CHECK(playbackGain(3.0f, 2.0f, true) == 2.0f) &&
        TEST_CHECK(playbackGain(0.5f, 2.0f, true) == 0.5f) &&
        TEST_CHECK(playbackGain(1.1f, std::numeric_limits<float>::infinity(), true) == 1.1f) &&
        TEST_CHECK(playbackGain(1.1f, 0.0f, false) == 1.1f) ? 0 : 1;
}
