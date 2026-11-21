#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace musik { namespace core { namespace audio {

    inline void copyPcmFrames(float* output, const float* input, std::size_t offset,
        std::size_t frames, std::size_t channels)
    {
        std::memcpy(output, input + offset * channels, frames * channels * sizeof(float));
    }

    inline void silencePcmFrames(float* output, std::size_t frames, std::size_t channels) {
        std::fill_n(output, frames * channels, 0.0f);
    }

} } }
