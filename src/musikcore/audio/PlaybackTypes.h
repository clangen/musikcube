#pragma once

namespace musik { namespace core { namespace audio {

    enum class PlaybackState: int {
        Stopped = 1,
        Paused = 2,
        Prepared = 3,
        Playing = 4,
    };

    enum class StreamState: int {
        Buffering = 1,
        Buffered = 2,
        Playing = 3,
        AlmostDone = 4,
        Finished = 5,
        Stopped = 6,
        Destroyed = 7,
        OpenFailed = -1
    };

    enum class RepeatMode: int {
        None = 0,
        Track = 1,
        List = 2
    };

    enum class TimeChangeMode: int {
        Seek = 0,
        Scrub = 1
    };

    enum class Capability: int {
        Prebuffer = 0x01
    };

    enum class TransportType: int {
        Gapless = 0,
        Crossfade = 1
    };

    enum class StreamFlags: int {
        None = 0,
        NoDSP = 1
    };

} } }
