#include "playback-storage.h"
#include "library-storage.h"
#include "../common/error.h"

#include <cmath>

mc_result MC_CALL mc_playback_get_snapshot(const mc_playback *playback, mc_playback_snapshot *snapshot) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback || !snapshot || snapshot->struct_size < sizeof(*snapshot)) return MC_ERROR_INVALID_ARGUMENT;

        mc_playback_snapshot result = MC_PLAYBACK_SNAPSHOT_DEFAULT;
        const auto status = playback->Snapshot(result);
        if (status == MC_OK) {
            result.struct_size = snapshot->struct_size;
            *snapshot = result;
        }
        return status;
    });
}

mc_result MC_CALL mc_playback_control(mc_playback *playback, mc_playback_command command, double value) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback) return MC_ERROR_INVALID_ARGUMENT;
        if (command < MC_PLAYBACK_TOGGLE_PAUSE || command > MC_PLAYBACK_SET_VOLUME) {
            return MC_ERROR_UNSUPPORTED;
        }
        if ((command == MC_PLAYBACK_SET_POSITION || command == MC_PLAYBACK_SET_VOLUME) &&
            (!std::isfinite(value) || value < 0 || (command == MC_PLAYBACK_SET_VOLUME && value > 1))) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return playback->Control(command, value);
    });
}

void MC_CALL mc_playback_destroy(mc_playback **playback) {
    if (!playback) return;
    delete *playback;
    *playback = nullptr;
}

mc_result MC_CALL mc_playback_play_result(mc_playback *playback, const mc_track_result *result, size_t row) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback || !result) return MC_ERROR_INVALID_ARGUMENT;
        const auto& data = *result->data;
        if (row >= data.ids.size()) return MC_ERROR_NOT_FOUND;
        return playback->PlayResult(data, row);
    });
}

static mc_result editQueue(mc_playback *playback, QueueEdit edit, size_t from = 0, size_t to = 0) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback) return MC_ERROR_INVALID_ARGUMENT;
        return playback->EditQueue(edit, from, to);
    });
}

mc_result MC_CALL mc_playback_queue_move(mc_playback *playback, size_t from, size_t to) {
    return editQueue(playback, QueueEdit::Move, from, to);
}

mc_result MC_CALL mc_playback_queue_remove(mc_playback *playback, size_t row) {
    return editQueue(playback, QueueEdit::Remove, row);
}

mc_result MC_CALL mc_playback_queue_clear(mc_playback *playback) {
    return editQueue(playback, QueueEdit::Clear);
}
