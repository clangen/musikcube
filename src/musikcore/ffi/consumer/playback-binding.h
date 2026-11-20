#pragma once

#include <musikcore/playback.hpp>
#include <musikcore/library/ILibrary.h>

struct LibraryIdentity;

namespace musik { namespace core { namespace audio {
    class PlaybackService;
    bool MatchesPlaybackOrigin(const LibraryIdentity& origin, const ILibraryPtr& library);

    /* Bind the public handle to the app's existing engine on its owner thread. */
    api::Playback CreatePlaybackHandle(PlaybackService& playback);

    /* The legacy queue stores one library's IDs. Drop it when that session ends. */
    void DiscardUnavailablePlaybackQueue(PlaybackService& playback);
} } }
