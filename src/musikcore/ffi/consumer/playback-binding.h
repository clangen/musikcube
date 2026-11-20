#pragma once

#include <musikcore/playback.hpp>

namespace musik { namespace core { namespace audio {
    class PlaybackService;

    /* Bind the public handle to the app's existing engine on its owner thread. */
    api::Playback CreatePlaybackHandle(PlaybackService& playback);

    /* The legacy queue stores one library's IDs. Drop it when that session ends. */
    void DiscardUnavailablePlaybackQueue(PlaybackService& playback);
} } }
