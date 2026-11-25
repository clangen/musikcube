#include "../../support/check.h"

#include <musikcore/audio/PlaybackMath.h>
#include <plugins/portaudioout/PcmFrames.h>

#include <array>
#include <limits>

using namespace musik::core::audio;

int main() {
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
