#include "playback-binding.h"

#include "library-storage.h"
#include "playback-storage.h"
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/support/Playback.h>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::core::sdk;

bool musik::core::audio::MatchesPlaybackOrigin(const LibraryIdentity& origin, const ILibraryPtr& library) {
    const auto master = std::dynamic_pointer_cast<MasterLibrary>(library);
    const auto selected = master ? master->Wrapped() : library;
    return origin.playbackOrigin && origin.playbackOrigin->Matches(selected.get());
}

/* Bridge the public handle to the existing engine until the API owns playback sessions. */
/* Borrows the owner-thread service. Neither copyable nor movable. */
class PlaybackService::PlaybackServiceBinding final : public mc_playback {
public:
    explicit PlaybackServiceBinding(PlaybackService& service)
        : service(service), lifetime(service.apiLifetime), owner(service.apiOwner) {}

    mc_result Control(mc_playback_command command, double value) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        switch (command) {
            case MC_PLAYBACK_TOGGLE_PAUSE: core::playback::PauseOrResume(this->service.GetTransport()); break;
            case MC_PLAYBACK_STOP: this->service.Stop(); break;
            case MC_PLAYBACK_NEXT: this->service.Next(); break;
            case MC_PLAYBACK_PREVIOUS: this->service.Previous(); break;
            case MC_PLAYBACK_SEEK_BACK: core::playback::SeekBack(this->service); break;
            case MC_PLAYBACK_SEEK_FORWARD: core::playback::SeekForward(this->service); break;
            case MC_PLAYBACK_SEEK_BACK_PROPORTIONAL: core::playback::SeekBackProportional(this->service); break;
            case MC_PLAYBACK_SEEK_FORWARD_PROPORTIONAL: core::playback::SeekForwardProportional(this->service); break;
            case MC_PLAYBACK_VOLUME_UP: core::playback::VolumeUp(this->service.GetTransport()); break;
            case MC_PLAYBACK_VOLUME_DOWN: core::playback::VolumeDown(this->service.GetTransport()); break;
            case MC_PLAYBACK_TOGGLE_MUTE: this->service.ToggleMute(); break;
            case MC_PLAYBACK_TOGGLE_REPEAT: this->service.ToggleRepeatMode(); break;
            case MC_PLAYBACK_TOGGLE_SHUFFLE: this->service.ToggleShuffle(); break;
            case MC_PLAYBACK_SET_POSITION: this->service.SetPosition(value); break;
            case MC_PLAYBACK_SET_VOLUME: this->service.SetVolume(value); break;
            default: return MC_ERROR_UNSUPPORTED;
        }
        return MC_OK;
    }

    mc_result Snapshot(mc_playback_snapshot& snapshot) const override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        switch (this->service.GetPlaybackState()) {
            case PlaybackState::Stopped: snapshot.state = MC_PLAYBACK_STOPPED; break;
            case PlaybackState::Paused: snapshot.state = MC_PLAYBACK_PAUSED; break;
            case PlaybackState::Prepared: snapshot.state = MC_PLAYBACK_PREPARED; break;
            case PlaybackState::Playing: snapshot.state = MC_PLAYBACK_PLAYING; break;
        }
        snapshot.position = this->service.GetPosition();
        snapshot.duration = this->service.GetTransport().GetDuration();
        snapshot.volume = this->service.GetVolume();
        snapshot.muted = this->service.IsMuted();
        return MC_OK;
    }

    mc_result PlayResult(const TrackResultData& result, size_t row) override {
        const auto status = this->CheckSession();
        return status == MC_OK
            ? this->PlayOrderedIds(result.library, result.ids.data(), result.ids.size(), row)
            : status;
    }

    mc_result UseTrack(const mc_track_ref& track, mc_playback_track_action action) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;
        if (action == MC_PLAYBACK_TRACK_PLAY_NOW) {
            return this->PlayOrderedIds(track.library, &track.id, 1, 0);
        }
        if (!track.library->playbackOrigin) return MC_ERROR_UNSUPPORTED;
        if (!MatchesPlaybackOrigin(*track.library, this->service.library)) return MC_ERROR_NOT_FOUND;

        /* The queue's IDs must all resolve through the same library. */
        auto editor = this->service.Edit();
        const auto& origin = this->service.queueOrigin;
        if (origin && !MatchesPlaybackOrigin(*origin, this->service.library)) return MC_ERROR_NOT_FOUND;

        const auto position = this->service.GetIndex();
        if (action == MC_PLAYBACK_TRACK_PREPEND) {
            if (!editor.Insert(track.id, 0)) return MC_ERROR_NOT_FOUND;
        }
        else if (action == MC_PLAYBACK_TRACK_PLAY_NEXT &&
            this->service.GetPlaybackState() != PlaybackState::Stopped &&
            position < this->service.playlist.Count()) {
            if (!editor.Insert(track.id, position + 1)) return MC_ERROR_NOT_FOUND;
        }
        else {
            editor.Add(track.id);
        }
        this->service.queueOrigin = track.library;
        return MC_OK;
    }

    mc_result EditQueue(QueueEdit edit, size_t from, size_t to) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        auto editor = this->service.Edit();
        if (edit == QueueEdit::Clear) {
            editor.Clear();
            return MC_OK;
        }
        const bool changed = edit == QueueEdit::Move ? editor.Move(from, to) : editor.Delete(from);
        return changed ? MC_OK : MC_ERROR_NOT_FOUND;
    }

private:
    mc_result PlayOrderedIds(const std::shared_ptr<const LibraryIdentity>& origin,
        const int64_t* ids, size_t count, size_t row) {
        if (!origin->playbackOrigin) return MC_ERROR_UNSUPPORTED;
        if (!MatchesPlaybackOrigin(*origin, this->service.library)) return MC_ERROR_NOT_FOUND;

        /* Install the snapshot directly in the engine's queue. */
        TrackList replacement(this->service.library, ids, count);
        {
            std::unique_lock<std::recursive_mutex> lock(this->service.playlistMutex);
            this->service.playlist.ClearCache();
            this->service.playlist.Swap(replacement);
            this->service.unshuffled.Clear();
            this->service.queueOrigin = origin;
        }
        this->service.Play(row);
        this->service.messageQueue.Post(Message::Create(&this->service, MESSAGE_NOTIFY_RESET, 0, 0));
        return MC_OK;
    }

    mc_result CheckSession() const {
        if (std::this_thread::get_id() != this->owner) return MC_ERROR_BUSY;
        if (this->lifetime.expired()) return MC_ERROR_NOT_FOUND;
        return MC_OK;
    }

    PlaybackService& service;
    const std::weak_ptr<int> lifetime;
    const std::thread::id owner;
};

musik::core::api::Playback musik::core::audio::CreatePlaybackHandle(PlaybackService& playback) {
    return api::Playback(new PlaybackService::PlaybackServiceBinding(playback));
}

void musik::core::audio::DiscardUnavailablePlaybackQueue(PlaybackService& playback) {
    std::unique_lock<std::recursive_mutex> lock(playback.playlistMutex);
    if (!playback.queueOrigin || MatchesPlaybackOrigin(*playback.queueOrigin, playback.library)) return;

    /* The legacy queue has one library. Its IDs cannot be used after the origin changes. */
    playback.Stop();
    playback.Edit().Clear();
}
