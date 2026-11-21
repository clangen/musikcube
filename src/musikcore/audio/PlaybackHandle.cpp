#include "PlaybackService.h"

#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/consumer/library-query-backends.h>
#include <musikcore/ffi/consumer/library-storage.h>
#include <musikcore/ffi/consumer/playback-storage.h>
#include <common/runtime/Message.h>
#include <musikcore/support/Playback.h>

#include <atomic>
#include <algorithm>
#include <mutex>
#include <utility>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::core::sdk;

namespace {
    struct QueueObserver {
        mc_play_queue_changed_fn callback;
        void* data;
        bool pending = false;
    };

    /* Serializes subscription changes with delivery. Recursive for self-unsubscription. */
    struct LiveObserver {
        std::recursive_mutex mutex;
        mc_playback_changed_fn callback = nullptr;
        void* data = nullptr;
        uint64_t generation = 0;
        std::vector<mc_playback_event> filter;
        std::vector<mc_playback_event> pending;

        void Set(mc_playback_changed_fn next, void* userData, std::vector<mc_playback_event> events = {}) {
            std::lock_guard lock(this->mutex);
            ++this->generation;
            this->pending.clear();
            this->callback = next;
            this->data = userData;
            this->filter = std::move(events);
        }

        void Deliver(uint64_t postedGeneration) {
            std::vector<mc_playback_event> events;
            {
                std::lock_guard lock(this->mutex);
                if (this->generation != postedGeneration) {
                    return;
                }
                events.swap(this->pending);
            }

            for (const auto event : events) {
                std::lock_guard lock(this->mutex);
                /* A callback may replace or disconnect this subscription. */
                if (this->generation != postedGeneration) {
                    return;
                }
                this->callback(this->data, event);
            }
        }
    };

    class PlaybackNotification final : public Message {
    public:
        PlaybackNotification(IMessageTarget* target, int type, std::function<void()> deliver)
            : Message(target, type, 0, 0), deliver(std::move(deliver)) {}

        std::function<void()> deliver;
    };
}

struct PlaybackService::PlaybackEvents {
    std::atomic<bool> active { true };
    std::mutex mutex;
    std::vector<std::weak_ptr<LiveObserver>> observers;

    void Add(const std::shared_ptr<LiveObserver>& observer) {
        std::lock_guard lock(this->mutex);
        std::erase_if(this->observers, [](const auto& slot) { return slot.expired(); });
        this->observers.push_back(observer);
    }

    std::vector<std::weak_ptr<LiveObserver>> Subscribers() {
        std::lock_guard lock(this->mutex);
        return this->observers;
    }
};

std::shared_ptr<PlaybackService::PlaybackEvents> PlaybackService::CreatePlaybackEvents() {
    return std::make_shared<PlaybackEvents>();
}

void PlaybackService::ClosePlaybackHandles() {
    this->apiEvents->active.store(false);
}

void PlaybackService::NotifyPlaybackChanged(mc_playback_event event) {
    if (!this->apiEvents->active.load()) {
        return;
    }

    /* Release the registry lock before touching any subscription. */
    for (const auto& weak : this->apiEvents->Subscribers()) {
        const auto observer = weak.lock();
        if (!observer) {
            continue;
        }

        std::lock_guard lock(observer->mutex);
        if (!observer->callback ||
            (!observer->filter.empty() && std::ranges::find(observer->filter, event) == observer->filter.end()) ||
            std::ranges::find(observer->pending, event) != observer->pending.end())
        {
            continue;
        }
        if (observer->pending.empty()) {
            this->messageQueue.Post(std::make_shared<PlaybackNotification>(this, MESSAGE_NOTIFY_API_CHANGED,
                [weak, generation = observer->generation] {
                    if (const auto current = weak.lock()) {
                        current->Deliver(generation);
                    }
                }));
        }
        observer->pending.push_back(event);
    }
}

void PlaybackService::DeliverPlaybackNotification(IMessage& message) {
    static_cast<PlaybackNotification&>(message).deliver();
}

bool PlaybackService::MatchesOrigin(const LibraryIdentity& origin) const {
    return origin.playbackOrigin && origin.playbackOrigin->Matches(this->library.get());
}

std::shared_ptr<const LibraryIdentity> PlaybackService::CaptureOrigin(int libraryId) const {
    if (this->library->Id() != libraryId) {
        return {};
    }

    std::shared_ptr<const LibraryIdentity> origin;
    ffi::exception_barrier(nullptr, [&] {
        origin = ffi::CaptureLibrary(this->library);
        return MC_OK;
    });
    return origin;
}

/* Borrows the owner-thread service. Neither copyable nor movable. */
class PlaybackService::PlaybackHandle final : public mc_playback, public sigslot::has_slots<> {
public:
    explicit PlaybackHandle(PlaybackService& service)
        : service(service), owner(service.apiOwner), events(service.apiEvents) {}

    ~PlaybackHandle() override {
        if (this->liveObserver) {
            this->liveObserver->Set(nullptr, nullptr);
        }
    }

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
            case MC_PLAYBACK_RELOAD_OUTPUT:
                this->service.ReloadOutput();
                break;
            default: return MC_ERROR_UNSUPPORTED;
        }
        return MC_OK;
    }

    mc_result Snapshot(mc_playback_snapshot& snapshot) const override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        const auto stream = this->service.GetTransport().GetStreamState();
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
        snapshot.shuffled = this->service.IsShuffled();
        if (const auto track = this->service.GetPlaying()) {
            const auto gain = track->GetReplayGain();
            snapshot.has_replay_gain = gain.albumGain != 1.0f || gain.albumPeak != 1.0f ||
                gain.trackGain != 1.0f || gain.trackPeak != 1.0f;
        }
        switch (this->service.GetRepeatMode()) {
            case RepeatMode::None:
                snapshot.repeat = MC_PLAYBACK_REPEAT_OFF;
                break;
            case RepeatMode::Track:
                snapshot.repeat = MC_PLAYBACK_REPEAT_TRACK;
                break;
            case RepeatMode::List:
                snapshot.repeat = MC_PLAYBACK_REPEAT_LIST;
                break;
        }
        switch (stream) {
            case StreamState::Stopped:
                snapshot.stream = MC_PLAYBACK_STREAM_STOPPED;
                break;
            case StreamState::Buffering:
                snapshot.stream = MC_PLAYBACK_STREAM_BUFFERING;
                break;
            case StreamState::Buffered:
                snapshot.stream = MC_PLAYBACK_STREAM_BUFFERED;
                break;
            case StreamState::Playing:
                snapshot.stream = MC_PLAYBACK_STREAM_PLAYING;
                break;
            case StreamState::AlmostDone:
                snapshot.stream = MC_PLAYBACK_STREAM_ALMOST_DONE;
                break;
            case StreamState::Finished:
                snapshot.stream = MC_PLAYBACK_STREAM_FINISHED;
                break;
            case StreamState::Destroyed:
                snapshot.stream = MC_PLAYBACK_STREAM_DESTROYED;
                break;
            case StreamState::OpenFailed:
                snapshot.stream = MC_PLAYBACK_STREAM_OPEN_FAILED;
                break;
        }
        return MC_OK;
    }

    mc_result CurrentTrack(mc_track_ref& track) const override {
        const auto status = this->CheckSession();
        if (status != MC_OK) {
            return status;
        }

        std::lock_guard lock(this->service.playlistMutex);
        const auto& origin = this->service.playingOrigin;
        const auto& playing = this->service.playingTrack;
        if (!this->PlayingTrackAvailable(playing, origin)) {
            return MC_ERROR_NOT_FOUND;
        }

        track = { origin, playing->GetId() };
        return MC_OK;
    }

    mc_result CurrentTrackMetadata(mc_track_page& page) const override {
        const auto status = this->CheckSession();
        if (status != MC_OK) {
            return status;
        }

        TrackPtr playing;
        {
            std::lock_guard lock(this->service.playlistMutex);
            const auto& origin = this->service.playingOrigin;
            playing = this->service.playingTrack;
            if (!this->PlayingTrackAvailable(playing, origin)) {
                return MC_ERROR_NOT_FOUND;
            }
        }

        /* Project the track playback loaded before publishing the change event. */
        page.names = { MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION };
        query::ProjectedTrackPage::Row row { playing->GetId(), {} };
        row.fields.reserve(page.names.size());
        for (const auto& field : page.names) {
            auto value = playing->GetString(field.c_str());
            if (value.empty()) {
                row.fields.emplace_back();
            }
            else {
                row.fields.push_back({ std::move(value) });
            }
        }
        page.rows.push_back(std::move(row));
        return MC_OK;
    }

    mc_result PlayResult(const TrackResultData& result, size_t row) override {
        const auto status = this->CheckSession();
        return status == MC_OK
            ? this->PlayOrderedIds(result.library, result.ids.data(), result.ids.size(), row)
            : status;
    }

    mc_result QueueSnapshot(mc_play_queue& queue) const override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        std::unique_lock<std::recursive_mutex> lock(this->service.playlistMutex);
        queue.lifetime = this->events;
        queue.revision = this->service.queueRevision;
        if (!this->QueueOriginAvailable()) return MC_OK;

        auto tracks = std::make_shared<TrackResultData>();
        tracks->ids = this->service.playlist.GetIds();
        tracks->library = this->service.queueOrigin;
        if (!tracks->library && !tracks->ids.empty()) {
            tracks->library = this->service.CaptureOrigin(this->service.library->Id());
        }
        queue.tracks = std::move(tracks);
        const auto row = this->service.GetIndex();
        if (row < queue.tracks->ids.size()) queue.retainedRow = row;
        const auto playing = this->service.GetPlaying();
        if (this->service.GetPlaybackState() != PlaybackState::Stopped &&
            playing && row < queue.tracks->ids.size() && queue.tracks->ids[row] == playing->GetId()) {
            queue.currentRow = row;
        }
        queue.shuffled = this->service.IsShuffled();
        return MC_OK;
    }

    mc_result ReplaceQueue(const TrackResultData& result) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;
        if (!result.library->playbackOrigin) return MC_ERROR_UNSUPPORTED;

        std::unique_lock<std::recursive_mutex> lock(this->service.playlistMutex);
        if (!this->service.MatchesOrigin(*result.library)) return MC_ERROR_NOT_FOUND;

        /* Preserve the legacy playlist-load behavior, including any active transport. */
        const TrackList replacement(this->service.library, result.ids.data(), result.ids.size());
        this->service.CopyFrom(replacement);
        this->service.queueOrigin = result.library;
        return MC_OK;
    }

    mc_result HotSwapQueue(const TrackResultData& result, size_t row) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) {
            return status;
        }
        if (!result.library->playbackOrigin) {
            return MC_ERROR_UNSUPPORTED;
        }

        std::unique_lock<std::recursive_mutex> lock(this->service.playlistMutex);
        if (row >= result.ids.size() || !this->service.MatchesOrigin(*result.library)) {
            return MC_ERROR_NOT_FOUND;
        }

        const TrackList replacement(this->service.library, result.ids.data(), result.ids.size());
        if (!this->service.HotSwap(replacement, row)) {
            return MC_ERROR_NOT_FOUND;
        }
        this->service.queueOrigin = result.library;
        return MC_OK;
    }

    mc_result SetQueueChangedCallback(mc_play_queue_changed_fn callback, void* data) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        if (callback && !this->observingQueue) {
            this->service.QueueEdited.connect(this, &PlaybackHandle::OnQueueChanged);
            this->service.TrackChanged.connect(this, &PlaybackHandle::OnTrackChanged);
            this->observingQueue = true;
        }
        /* Replacing a subscription invalidates any delivery posted for its user data. */
        this->queueObserver = callback ? std::make_shared<QueueObserver>(QueueObserver { callback, data }) : nullptr;
        return MC_OK;
    }

    mc_result SetChangedCallback(mc_playback_changed_fn callback, void* data,
        std::vector<mc_playback_event> filter) override
    {
        const auto events = this->events.lock();
        if (!events || !events->active.load()) {
            return MC_ERROR_NOT_FOUND;
        }

        std::shared_ptr<LiveObserver> observer;
        {
            std::lock_guard lock(this->liveRegistrationMutex);
            if (callback && !this->liveObserver) {
                auto slot = std::make_shared<LiveObserver>();
                events->Add(slot);
                this->liveObserver = std::move(slot);
            }
            observer = this->liveObserver;
        }
        if (observer) {
            observer->Set(callback, data, std::move(filter));
        }
        return MC_OK;
    }

    mc_result PlayQueueRow(const mc_play_queue& queue, size_t row) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        std::unique_lock<std::recursive_mutex> lock(this->service.playlistMutex);
        if (!this->QueueIsCurrent(queue)) return MC_ERROR_STALE_SNAPSHOT;
        if (!queue.tracks || row >= queue.tracks->ids.size()) return MC_ERROR_NOT_FOUND;
        this->service.Play(row);
        return MC_OK;
    }

    mc_result UseTrack(const mc_track_ref& track, mc_playback_track_action action) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;
        if (action == MC_PLAYBACK_TRACK_PLAY_NOW) {
            return this->PlayOrderedIds(track.library, &track.id, 1, 0);
        }
        return this->InsertOrderedIds(track.library, &track.id, 1, action);
    }

    mc_result InsertResult(const TrackResultData& result, mc_playback_track_action action) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) {
            return status;
        }
        return this->InsertOrderedIds(result.library, result.ids.data(), result.ids.size(), action);
    }

    mc_result EditQueue(QueueEdit edit, const mc_play_queue* queue, size_t from, size_t to) override {
        const auto status = this->CheckSession();
        if (status != MC_OK) return status;

        auto editor = this->service.Edit();
        if (edit == QueueEdit::Clear) {
            editor.Clear();
            return MC_OK;
        }
        if (!this->QueueIsCurrent(*queue)) return MC_ERROR_STALE_SNAPSHOT;
        const bool changed = edit == QueueEdit::Move ? editor.Move(from, to) : editor.Delete(from);
        return changed ? MC_OK : MC_ERROR_NOT_FOUND;
    }

private:
    bool PlayingTrackAvailable(const TrackPtr& track, const std::shared_ptr<const LibraryIdentity>& origin) const {
        if (!track || !origin || !this->service.MatchesOrigin(*origin)) {
            return false;
        }
        const auto stream = this->service.GetTransport().GetStreamState();
        return stream == StreamState::Buffering || stream == StreamState::Buffered ||
            this->service.GetPlaybackState() != PlaybackState::Stopped;
    }

    bool QueueOriginAvailable() const {
        const auto& origin = this->service.queueOrigin;
        return !origin || this->service.MatchesOrigin(*origin);
    }

    /* Check under the same lock used for the positional action. */
    bool QueueIsCurrent(const mc_play_queue& queue) const {
        return queue.lifetime.lock() == this->service.apiEvents &&
            queue.revision == this->service.queueRevision && this->QueueOriginAvailable();
    }

    void OnQueueChanged() {
        if (this->CheckSession() != MC_OK || !this->queueObserver || this->queueObserver->pending) {
            return;
        }

        /* The legacy signal holds its lock while notifying listeners. */
        this->service.messageQueue.Post(std::make_shared<PlaybackNotification>(
            &this->service, MESSAGE_NOTIFY_API_CHANGED, [weak = std::weak_ptr(this->queueObserver)] {
                if (const auto observer = weak.lock()) {
                    observer->pending = false;
                    observer->callback(observer->data);
                }
            }));
        this->queueObserver->pending = true;
    }

    void OnTrackChanged(size_t, TrackPtr) {
        this->OnQueueChanged();
    }

    mc_result InsertOrderedIds(const std::shared_ptr<const LibraryIdentity>& origin,
        const int64_t* ids, size_t count, mc_playback_track_action action)
    {
        if (!origin->playbackOrigin) {
            return MC_ERROR_UNSUPPORTED;
        }
        auto editor = this->service.Edit();
        if (!this->service.MatchesOrigin(*origin) || !this->QueueOriginAvailable()) {
            return MC_ERROR_NOT_FOUND;
        }
        if (!count) {
            return MC_OK;
        }

        const auto position = this->service.GetIndex();
        const bool prepend = action == MC_PLAYBACK_TRACK_PREPEND;
        const bool next = action == MC_PLAYBACK_TRACK_PLAY_NEXT && position < this->service.playlist.Count();
        if (prepend || next) {
            const auto first = prepend ? 0 : position + 1;
            for (size_t offset = 0; offset < count; ++offset) {
                if (!editor.Insert(ids[offset], first + offset)) {
                    return MC_ERROR_NOT_FOUND;
                }
            }
        }
        else {
            for (size_t offset = 0; offset < count; ++offset) {
                editor.Add(ids[offset]);
            }
        }
        this->service.queueOrigin = origin;
        return MC_OK;
    }

    mc_result PlayOrderedIds(const std::shared_ptr<const LibraryIdentity>& origin,
        const int64_t* ids, size_t count, size_t row) {
        if (!origin->playbackOrigin) return MC_ERROR_UNSUPPORTED;
        if (!this->service.MatchesOrigin(*origin)) return MC_ERROR_NOT_FOUND;

        /* Install the snapshot directly in the engine's queue. */
        TrackList replacement(this->service.library, ids, count);
        {
            std::unique_lock<std::recursive_mutex> lock(this->service.playlistMutex);
            this->service.playlist.ClearCache();
            this->service.playlist.Swap(replacement);
            this->service.unshuffled.Clear();
            this->service.queueOrigin = origin;
            ++this->service.queueRevision;
        }
        this->service.Play(row);
        this->service.messageQueue.Post(Message::Create(&this->service, MESSAGE_NOTIFY_RESET, 0, 0));
        return MC_OK;
    }

    mc_result CheckSession() const {
        if (std::this_thread::get_id() != this->owner) {
            return MC_ERROR_WRONG_THREAD;
        }
        const auto events = this->events.lock();
        return events && events->active.load() ? MC_OK : MC_ERROR_NOT_FOUND;
    }

    PlaybackService& service;
    const std::thread::id owner;
    const std::weak_ptr<PlaybackEvents> events;
    std::mutex liveRegistrationMutex;
    std::shared_ptr<LiveObserver> liveObserver;
    std::shared_ptr<QueueObserver> queueObserver;
    bool observingQueue = false;
};

mc_result PlaybackService::OpenHandle(mc_playback** out) {
    if (!out || *out) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    if (std::this_thread::get_id() != this->apiOwner) {
        return MC_ERROR_WRONG_THREAD;
    }
    return ffi::exception_barrier(nullptr, [&] {
        *out = new PlaybackHandle(*this);
        return MC_OK;
    });
}

void PlaybackService::InvalidateDisconnectedCatalog() {
    {
        std::lock_guard lock(this->playlistMutex);
        const bool staleQueue = this->queueOrigin && !this->MatchesOrigin(*this->queueOrigin);
        const bool staleTrack = this->playingOrigin && !this->MatchesOrigin(*this->playingOrigin);
        if (!staleQueue && !staleTrack) {
            return;
        }

        /* Reconnecting does not make a prior connection's queue IDs valid. */
        this->Stop();
        this->Edit().Clear();
    }

    /* A terminal stream event can be filtered once its catalog is disconnected. */
    this->OnTrackChanged(SIZE_MAX, {});
}
