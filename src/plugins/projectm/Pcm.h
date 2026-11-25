#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

/* projectM expects samples between -1 and 1. report the input peak before
applying gain so the control can also help diagnose a quiet source. */
inline float ScaleVisualizerPcm(float* samples, size_t count, float gain) noexcept {
    float peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peak = std::max(peak, std::abs(samples[i]));
        samples[i] = std::clamp(samples[i] * gain, -1.0f, 1.0f);
    }
    return peak;
}
