#pragma once

#include <musikcore/playback.h>

#include <memory>

struct LibraryIdentity;
struct TrackResultData;

enum class QueueEdit { Move, Remove, Clear };

/* Private polymorphic handle. Neither copyable nor movable. */
struct mc_playback {
    mc_playback() = default;
    virtual ~mc_playback() = default;
    mc_playback(const mc_playback&) = delete;
    mc_playback& operator=(const mc_playback&) = delete;

    virtual mc_result Control(mc_playback_command command, double value) = 0;
    virtual mc_result Snapshot(mc_playback_snapshot& snapshot) const = 0;
    virtual mc_result PlayResult(const TrackResultData& result, size_t row) = 0;
    virtual mc_result EditQueue(QueueEdit edit, size_t from, size_t to) = 0;
};
