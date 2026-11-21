#pragma once

#include <musikcore/playback.hpp>
#include <musikcore/library/ILibrary.h>

struct LibraryIdentity;
namespace musik { namespace core { namespace runtime { class IMessage; } } }

namespace musik { namespace core { namespace audio {
    class PlaybackService;
    bool MatchesPlaybackOrigin(const LibraryIdentity& origin, const ILibraryPtr& library);
    std::shared_ptr<const LibraryIdentity> CapturePlaybackOrigin(const ILibraryPtr& library, int libraryId);
    void DeliverPlaybackNotification(runtime::IMessage& message);

    /* Bind the public handle to the app's existing engine on its owner thread. */
    api::Playback CreatePlaybackHandle(PlaybackService& playback);
    /* Call on the engine's owner thread. Playback must outlive the context. */
    void AttachPlayback(mc_context& context, PlaybackService& playback);

    /* The legacy queue stores one library's IDs. Drop it when that session ends. */
    void DiscardUnavailablePlaybackQueue(PlaybackService& playback);
} } }
