#pragma once

#include <algorithm>
#include <cmath>

namespace musik { namespace core { namespace audio {

    inline float playbackGain(float requested, float peakCeiling, bool peakValid) {
        if (requested > 1.0f && peakValid && std::isfinite(peakCeiling) && peakCeiling > 0.0f) {
            return std::min(requested, peakCeiling);
        }
        return requested;
    }

    inline int pcmBufferCount(double seconds, int sampleRate, int framesPerBuffer, int minimum) {
        return std::max(minimum, static_cast<int>(std::ceil(seconds * sampleRate / framesPerBuffer)));
    }

} } }
