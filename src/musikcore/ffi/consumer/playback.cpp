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

mc_result MC_CALL mc_playback_get_queue(const mc_playback *playback, mc_play_queue **out_queue) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_queue || *out_queue) return MC_ERROR_INVALID_ARGUMENT;
        if (!playback) return MC_ERROR_INVALID_ARGUMENT;

        std::unique_ptr<mc_play_queue> queue(new mc_play_queue());
        const auto status = playback->QueueSnapshot(*queue);
        if (status == MC_OK) *out_queue = queue.release();
        return status;
    });
}

void MC_CALL mc_play_queue_destroy(mc_play_queue **queue) {
    if (!queue) return;
    delete *queue;
    *queue = nullptr;
}

const int64_t *MC_CALL mc_play_queue_get_ids(const mc_play_queue *queue, size_t *out_count) {
    if (out_count) *out_count = queue ? queue->ids.size() : 0;
    return queue && !queue->ids.empty() ? queue->ids.data() : nullptr;
}

size_t MC_CALL mc_play_queue_get_current_row(const mc_play_queue *queue) {
    return queue ? queue->currentRow : SIZE_MAX;
}

bool MC_CALL mc_play_queue_is_shuffled(const mc_play_queue *queue) {
    return queue && queue->shuffled;
}

size_t MC_CALL mc_play_queue_get_retained_row(const mc_play_queue *queue) {
    return queue ? queue->retainedRow : SIZE_MAX;
}

mc_result MC_CALL mc_playback_replace_queue(mc_playback *playback, const mc_track_result *result) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback || !result) return MC_ERROR_INVALID_ARGUMENT;
        return playback->ReplaceQueue(*result->data);
    });
}

mc_result MC_CALL mc_playback_set_queue_changed_callback(
    mc_playback *playback, mc_play_queue_changed_fn callback, void *user_data) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback) return MC_ERROR_INVALID_ARGUMENT;
        return playback->SetQueueChangedCallback(callback, user_data);
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

mc_result MC_CALL mc_playback_use_track(
    mc_playback *playback, const mc_track_ref *track, mc_playback_track_action action) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback || !track) return MC_ERROR_INVALID_ARGUMENT;
        if (action < MC_PLAYBACK_TRACK_PLAY_NOW || action > MC_PLAYBACK_TRACK_PREPEND) return MC_ERROR_UNSUPPORTED;
        return playback->UseTrack(*track, action);
    });
}

mc_result MC_CALL mc_playback_play_queue_row(mc_playback *playback, const mc_play_queue *queue, size_t row) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback || !queue) return MC_ERROR_INVALID_ARGUMENT;
        return playback->PlayQueueRow(*queue, row);
    });
}

static mc_result editQueue(mc_playback *playback, QueueEdit edit, const mc_play_queue *queue, size_t from, size_t to) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!playback || (!queue && edit != QueueEdit::Clear)) return MC_ERROR_INVALID_ARGUMENT;
        return playback->EditQueue(edit, queue, from, to);
    });
}

mc_result MC_CALL mc_playback_queue_move(mc_playback *playback, const mc_play_queue *queue, size_t from, size_t to) {
    return editQueue(playback, QueueEdit::Move, queue, from, to);
}

mc_result MC_CALL mc_playback_queue_remove(mc_playback *playback, const mc_play_queue *queue, size_t row) {
    return editQueue(playback, QueueEdit::Remove, queue, row, 0);
}

mc_result MC_CALL mc_playback_queue_clear(mc_playback *playback) {
    return editQueue(playback, QueueEdit::Clear, nullptr, 0, 0);
}
