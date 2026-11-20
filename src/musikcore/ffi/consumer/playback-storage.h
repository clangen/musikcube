#pragma once

#include <musikcore/playback.h>

#include <memory>
#include <vector>

struct LibraryIdentity;
struct TrackResultData;

enum class QueueEdit { Move, Remove, Clear };

struct mc_play_queue {
    std::vector<int64_t> ids;
    std::weak_ptr<int> lifetime;
    uint64_t revision = 0;
    size_t currentRow = SIZE_MAX;
    size_t retainedRow = SIZE_MAX;
    bool shuffled = false;
};

/* Private polymorphic handle. Neither copyable nor movable. */
struct mc_playback {
    mc_playback() = default;
    virtual ~mc_playback() = default;
    mc_playback(const mc_playback&) = delete;
    mc_playback& operator=(const mc_playback&) = delete;

    virtual mc_result Control(mc_playback_command command, double value) = 0;
    virtual mc_result Snapshot(mc_playback_snapshot& snapshot) const = 0;
    virtual mc_result QueueSnapshot(mc_play_queue& queue) const = 0;
    virtual mc_result SetQueueChangedCallback(mc_play_queue_changed_fn callback, void* data) = 0;
    virtual mc_result PlayResult(const TrackResultData& result, size_t row) = 0;
    virtual mc_result ReplaceQueue(const TrackResultData& result) = 0;
    virtual mc_result PlayQueueRow(const mc_play_queue& queue, size_t row) = 0;
    virtual mc_result UseTrack(const mc_track_ref& track, mc_playback_track_action action) = 0;
    virtual mc_result EditQueue(QueueEdit edit, const mc_play_queue* queue, size_t from, size_t to) = 0;
};
