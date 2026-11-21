#pragma once

#include <cstddef>

namespace playback_fixture {

    inline constexpr char driver[] = "Capture";
    inline constexpr int sampleRate = 48000;
    inline constexpr char stalledSource[] = "fixture://stalled-source.wav";

    struct CapturedFrame {
        float left {};
        float right {};
        bool underrun {};
        bool interrupted {};
    };

    using ReadCapture = std::size_t (*)(CapturedFrame*, std::size_t);

    enum class CaptureEvent { HeldCompletion, Stop, Resumed, Balanced, Overlap, SourceOpening, SourceInterrupted };
    using HoldCompletion = void (*)(bool);
    using WaitCapture = bool (*)(CaptureEvent, unsigned);
    using WaitCaptureFrames = bool (*)(std::size_t, unsigned);
    using CountCapture = std::size_t (*)();
    using StoreCapture = void (*)(bool);
}
