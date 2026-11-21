#ifndef MUSIKCORE_PLAYBACK_HPP
#define MUSIKCORE_PLAYBACK_HPP

#include <musikcore/playback.h>
#include <musikcore/library.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace musik { namespace core { namespace api {

    /* Immutable snapshot of queue IDs and current/retained rows. Use it to
       display the queue or identify positions for edits; capture a new snapshot
       after edits because stale positional actions are rejected. Move-only,
       transferring ID storage; the Ids() array lasts until its owner releases it. */
    class PlayQueue {
    public:
        explicit PlayQueue(mc_play_queue* queue) noexcept : queue(queue) {}
        PlayQueue(const PlayQueue&) = delete;
        PlayQueue& operator=(const PlayQueue&) = delete;
        PlayQueue(PlayQueue&&) noexcept = default;
        PlayQueue& operator=(PlayQueue&&) noexcept = default;

        /* The returned array is owned by this snapshot. */
        const int64_t* Ids(size_t& count) const & noexcept {
            return mc_play_queue_get_ids(this->queue.get(), &count);
        }
        const int64_t* Ids(size_t& count) const && = delete;
        size_t Size() const noexcept {
            size_t count = 0;
            this->Ids(count);
            return count;
        }
        size_t CurrentRow() const noexcept { return mc_play_queue_get_current_row(this->queue.get()); }
        size_t RetainedRow() const noexcept { return mc_play_queue_get_retained_row(this->queue.get()); }
        bool IsShuffled() const noexcept { return mc_play_queue_is_shuffled(this->queue.get()); }

    private:
        friend class Playback;

        /* Releases only the captured C snapshot, without changing playback.
           Stateless; copy and move leave no retained state. */
        struct Deleter {
            void operator()(mc_play_queue* queue) const noexcept { mc_play_queue_destroy(&queue); }
        };
        std::unique_ptr<mc_play_queue, Deleter> queue;
    };

    /* Controls and samples an existing app-supplied playback session on its
       owner thread. Use for transport commands and queue edits; it does not
       create an audio engine. Move-only, transferring handle ownership.
       Releasing the handle does not stop playback or destroy the session. */
    class Playback {
    public:
        /* Takes ownership of a handle supplied by core. */
        explicit Playback(mc_playback* handle) noexcept : handle(handle) {}
        Playback(const Playback&) = delete;
        Playback& operator=(const Playback&) = delete;
        Playback(Playback&&) noexcept = default;
        Playback& operator=(Playback&&) noexcept = default;

        /* Wrapper-owned C handle. Do not destroy it; valid until this wrapper releases it. */
        mc_playback* Get() noexcept { return this->handle.get(); }
        const mc_playback* Get() const noexcept { return this->handle.get(); }

        mc_result Control(mc_playback_command command, double value = 0) {
            return mc_playback_control(this->handle.get(), command, value);
        }

        std::optional<mc_playback_snapshot> Snapshot() const {
            mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
            const auto status = mc_playback_get_snapshot(this->handle.get(), &snapshot);
            if (status == MC_ERROR_NOT_FOUND) return std::nullopt;
            if (status != MC_OK) throw Error(status);
            return snapshot;
        }

        std::optional<PlayQueue> Queue() const {
            mc_play_queue* queue = nullptr;
            const auto status = mc_playback_get_queue(this->handle.get(), &queue);
            if (status == MC_ERROR_NOT_FOUND) return std::nullopt;
            if (status != MC_OK) throw Error(status);
            return PlayQueue(queue);
        }

        std::optional<TrackRef> CurrentTrack() const {
            mc_track_ref* track = nullptr;
            const auto status = mc_playback_get_current_track(this->handle.get(), &track);
            if (status == MC_ERROR_NOT_FOUND) {
                return std::nullopt;
            }
            if (status != MC_OK) {
                throw Error(status);
            }
            return TrackRef(track);
        }

        mc_result SetQueueChangedCallback(mc_play_queue_changed_fn callback, void* data = nullptr) {
            return mc_playback_set_queue_changed_callback(this->handle.get(), callback, data);
        }

        mc_result SetChangedCallback(mc_playback_changed_fn callback, void* data = nullptr,
            const std::vector<mc_playback_event>& events = {})
        {
            return mc_playback_set_changed_callback(this->handle.get(), callback, data, events.data(), events.size());
        }

        mc_result Play(const TrackResult& result, size_t row) {
            return mc_playback_play_result(this->handle.get(), result.Get(), row);
        }
        mc_result PlayQueueRow(const PlayQueue& queue, size_t row) {
            return mc_playback_play_queue_row(this->handle.get(), queue.queue.get(), row);
        }
        mc_result ReplaceQueue(const TrackResult& result) {
            return mc_playback_replace_queue(this->handle.get(), result.Get());
        }

        mc_result HotSwapQueue(const TrackResult& result, size_t row) {
            return mc_playback_hot_swap_queue(this->handle.get(), result.Get(), row);
        }

        mc_result InsertResult(const TrackResult& result, mc_playback_track_action action) {
            return mc_playback_insert_result(this->handle.get(), result.Get(), action);
        }

        mc_result Move(const PlayQueue& queue, size_t from, size_t to) {
            return mc_playback_queue_move(this->handle.get(), queue.queue.get(), from, to);
        }
        mc_result UseTrack(const TrackRef& track, mc_playback_track_action action) {
            return mc_playback_use_track(this->handle.get(), track.Get(), action);
        }
        mc_result Remove(const PlayQueue& queue, size_t row) {
            return mc_playback_queue_remove(this->handle.get(), queue.queue.get(), row);
        }
        mc_result ClearQueue() { return mc_playback_queue_clear(this->handle.get()); }

    private:
        /* Releases the C handle and its subscription, leaving playback active.
           Stateless; copying or moving it does not affect handle ownership. */
        struct Deleter {
            void operator()(mc_playback* handle) const noexcept { mc_playback_destroy(&handle); }
        };
        std::unique_ptr<mc_playback, Deleter> handle;
    };

    inline Playback Context::GetPlayback() const {
        mc_playback* playback = nullptr;
        const auto status = mc_context_get_playback(this->context, &playback);
        if (status != MC_OK) {
            throw Error(status);
        }
        return Playback(playback);
    }

    /* Owns a playback handle and its live subscription. Destruction drops
       pending delivery. Use a separate handle for each subscriber.
       Neither copyable nor movable; the callback uses this object's address. */
    class PlaybackEventSubscription {
    public:
        using Callback = std::function<void(mc_playback_event)>;

        PlaybackEventSubscription(Playback playback, Callback callback, const std::vector<mc_playback_event>& events = {})
            : callback(std::make_shared<Callback>(std::move(callback))), playback(std::move(playback))
        {
            if (!*this->callback) {
                throw Error(MC_ERROR_INVALID_ARGUMENT);
            }
            const auto status = this->playback.SetChangedCallback(Notify, this, events);
            if (status != MC_OK) {
                throw Error(status);
            }
        }
        PlaybackEventSubscription(const PlaybackEventSubscription&) = delete;
        PlaybackEventSubscription(PlaybackEventSubscription&&) = delete;

        PlaybackEventSubscription& operator=(const PlaybackEventSubscription&) = delete;
        PlaybackEventSubscription& operator=(PlaybackEventSubscription&&) = delete;

    private:
        static void MC_CALL Notify(void* data, mc_playback_event event) {
            /* The callback may destroy its own subscription. */
            const auto callback = static_cast<PlaybackEventSubscription*>(data)->callback;
            (*callback)(event);
        }

        std::shared_ptr<const Callback> callback;
        Playback playback;
    };

    inline mc_playback_gain_settings ReadPlaybackGainSettings() {
        mc_playback_gain_settings settings {};
        const auto result = mc_playback_get_gain_settings(&settings);
        if (result != MC_OK && result != MC_ERROR_UNSUPPORTED) {
            throw Error(result);
        }
        return settings;
    }

    inline mc_result SetPreamp(double decibels) {
        return mc_playback_set_preamp(decibels);
    }

    inline mc_result SetReplayGain(mc_replay_gain_mode mode) {
        return mc_playback_set_replay_gain(mode);
    }
} } }

#endif
