#pragma once

#include <algorithm>
#include <cstddef>

/* projectM expects samples between -1 and 1. */
inline void ScaleVisualizerPcm(float* samples, size_t count, float gain) noexcept {
    for (size_t i = 0; i < count; ++i) {
        samples[i] = std::clamp(samples[i] * gain, -1.0f, 1.0f);
    }
}
