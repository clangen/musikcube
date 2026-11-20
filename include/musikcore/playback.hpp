#ifndef MUSIKCORE_PLAYBACK_HPP
#define MUSIKCORE_PLAYBACK_HPP

#include <musikcore/playback.h>
#include <musikcore/library.hpp>

#include <memory>
#include <optional>

namespace musik { namespace core { namespace api {

    /* Move-only snapshot. Moving transfers ownership of its track ID storage. */
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

        /* Stateless deleter. Copy and move leave no retained state. */
        struct Deleter {
            void operator()(mc_play_queue* queue) const noexcept { mc_play_queue_destroy(&queue); }
        };
        std::unique_ptr<mc_play_queue, Deleter> queue;
    };

    /* Move-only handle to a playback session. Moving transfers handle ownership. */
    class Playback {
    public:
        /* Takes ownership of a handle supplied by core. */
        explicit Playback(mc_playback* handle) noexcept : handle(handle) {}
        Playback(const Playback&) = delete;
        Playback& operator=(const Playback&) = delete;
        Playback(Playback&&) noexcept = default;
        Playback& operator=(Playback&&) noexcept = default;

        mc_result Control(mc_playback_command command, double value = 0) {
            return mc_playback_control(this->handle.get(), command, value);
        }

        std::optional<mc_playback_snapshot> Snapshot() const {
            mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
            if (mc_playback_get_snapshot(this->handle.get(), &snapshot) != MC_OK) return std::nullopt;
            return snapshot;
        }

        std::optional<PlayQueue> Queue() const {
            mc_play_queue* queue = nullptr;
            if (mc_playback_get_queue(this->handle.get(), &queue) != MC_OK) return std::nullopt;
            return PlayQueue(queue);
        }

        mc_result SetQueueChangedCallback(mc_play_queue_changed_fn callback, void* data = nullptr) {
            return mc_playback_set_queue_changed_callback(this->handle.get(), callback, data);
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
        /* Stateless deleter. Copying or moving it does not affect handle ownership. */
        struct Deleter {
            void operator()(mc_playback* handle) const noexcept { mc_playback_destroy(&handle); }
        };
        std::unique_ptr<mc_playback, Deleter> handle;
    };

} } }

#endif
