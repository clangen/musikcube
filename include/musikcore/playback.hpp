#ifndef MUSIKCORE_PLAYBACK_HPP
#define MUSIKCORE_PLAYBACK_HPP

#include <musikcore/playback.h>
#include <musikcore/library.hpp>

#include <memory>
#include <optional>

namespace musik { namespace core { namespace api {

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

        mc_result Play(const TrackResult& result, size_t row) {
            return mc_playback_play_result(this->handle.get(), result.Get(), row);
        }

        mc_result Move(size_t from, size_t to) { return mc_playback_queue_move(this->handle.get(), from, to); }
        mc_result Remove(size_t row) { return mc_playback_queue_remove(this->handle.get(), row); }
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
