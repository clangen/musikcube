#pragma once

#include <cstddef>

namespace playback_fixture {

    inline constexpr char driver[] = "Capture";
    inline constexpr int sampleRate = 48000;

    struct CapturedFrame {
        float left {};
        float right {};
        bool underrun {};
        bool interrupted {};
    };

    using ReadCapture = std::size_t (*)(CapturedFrame*, std::size_t);
}
