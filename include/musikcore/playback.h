#ifndef MUSIKCORE_PLAYBACK_H
#define MUSIKCORE_PLAYBACK_H

#include <musikcore/abi.h>
#include <musikcore/library.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* An app-supplied playback session handle. The standalone library has no playback engine. */
typedef struct mc_playback mc_playback;

typedef uint32_t mc_playback_command;
enum {
    MC_PLAYBACK_TOGGLE_PAUSE = 1,
    MC_PLAYBACK_STOP,
    MC_PLAYBACK_NEXT,
    MC_PLAYBACK_PREVIOUS,
    MC_PLAYBACK_SEEK_BACK,
    MC_PLAYBACK_SEEK_FORWARD,
    MC_PLAYBACK_SEEK_BACK_PROPORTIONAL,
    MC_PLAYBACK_SEEK_FORWARD_PROPORTIONAL,
    MC_PLAYBACK_VOLUME_UP,
    MC_PLAYBACK_VOLUME_DOWN,
    MC_PLAYBACK_TOGGLE_MUTE,
    MC_PLAYBACK_TOGGLE_REPEAT,
    MC_PLAYBACK_TOGGLE_SHUFFLE,
    MC_PLAYBACK_SET_POSITION,
    MC_PLAYBACK_SET_VOLUME
};

typedef uint32_t mc_playback_state;
enum {
    MC_PLAYBACK_STOPPED = 1,
    MC_PLAYBACK_PAUSED,
    MC_PLAYBACK_PREPARED,
    MC_PLAYBACK_PLAYING
};

typedef struct mc_playback_snapshot {
    /* Size of this structure in bytes. Initialize with MC_PLAYBACK_SNAPSHOT_DEFAULT. */
    uint32_t struct_size;
    /* Current transport state. */
    mc_playback_state state;
    /* Current playback position or pending seek position in seconds. */
    double position;
    /* Track duration in seconds. A nonpositive value means the duration is unknown. */
    double duration;
    /* Current volume from zero to one. */
    double volume;
    /* True when output is muted. */
    bool muted;
} mc_playback_snapshot;

#define MC_PLAYBACK_SNAPSHOT_DEFAULT { sizeof(mc_playback_snapshot), MC_PLAYBACK_STOPPED, 0, 0, 0, false }

/* Sample the session on its owner thread. On failure, snapshot remains unchanged. */
MC_API mc_result MC_CALL mc_playback_get_snapshot(const mc_playback *playback, mc_playback_snapshot *snapshot);

/* Apply a command to an existing playback session on its owner thread. */
/* Value is seconds for SET_POSITION, or a fraction from zero to one for SET_VOLUME. */
/* Other commands ignore value. */
/* Returns MC_ERROR_BUSY on another thread or MC_ERROR_NOT_FOUND after the session ends. */
MC_API mc_result MC_CALL mc_playback_control(mc_playback *playback, mc_playback_command command, double value);

/* Replace the queue with the ordered result and play from row on the owner thread. */
/* Returns MC_ERROR_NOT_FOUND if the row or its original library session is unavailable. */
MC_API mc_result MC_CALL mc_playback_play_result(mc_playback *playback, const mc_track_result *result, size_t row);

typedef uint32_t mc_playback_track_action;
enum {
    MC_PLAYBACK_TRACK_PLAY_NOW = 1,
    MC_PLAYBACK_TRACK_PLAY_NEXT,
    MC_PLAYBACK_TRACK_APPEND,
    MC_PLAYBACK_TRACK_PREPEND
};

/* Play now replaces the queue. Play next inserts after the current track, or appends if stopped. */
/* Call on the owner thread. The track is borrowed for the call. */
/* Returns MC_ERROR_NOT_FOUND if the originating library session is unavailable. */
MC_API mc_result MC_CALL mc_playback_use_track(
    mc_playback *playback, const mc_track_ref *track, mc_playback_track_action action);

typedef struct mc_play_queue mc_play_queue;

/* Capture the ordered queue on the owner thread. The caller owns the returned snapshot. */
MC_API mc_result MC_CALL mc_playback_get_queue(const mc_playback *playback, mc_play_queue **out_queue);
MC_API void MC_CALL mc_play_queue_destroy(mc_play_queue **queue);
/* Borrow the ordered track IDs until the snapshot is destroyed. Duplicates are preserved. */
MC_API const int64_t *MC_CALL mc_play_queue_get_ids(const mc_play_queue *queue, size_t *out_count);
/* Returns SIZE_MAX when the queue has no current row. */
MC_API size_t MC_CALL mc_play_queue_get_current_row(const mc_play_queue *queue);
MC_API bool MC_CALL mc_play_queue_is_shuffled(const mc_play_queue *queue);
/* Start the existing queue at row without replacing it. Call on the owner thread. */
MC_API mc_result MC_CALL mc_playback_play_queue_row(mc_playback *playback, size_t row);

typedef void (MC_CALL *mc_play_queue_changed_fn)(void *user_data);
/* Register on the owner thread. The playback dispatcher delivers queue and current-row changes. */
/* Pass NULL to unsubscribe. Keep user_data alive until unsubscription or handle destruction. */
/* Do not destroy the playback handle from inside the callback. */
MC_API mc_result MC_CALL mc_playback_set_queue_changed_callback(
    mc_playback *playback, mc_play_queue_changed_fn callback, void *user_data);

/* Move or remove an existing entry by zero-based queue position on the owner thread. */
MC_API mc_result MC_CALL mc_playback_queue_move(mc_playback *playback, size_t from, size_t to);
MC_API mc_result MC_CALL mc_playback_queue_remove(mc_playback *playback, size_t row);
MC_API mc_result MC_CALL mc_playback_queue_clear(mc_playback *playback);
/* Release the handle. The playback session and its queue remain active. */
MC_API void MC_CALL mc_playback_destroy(mc_playback **playback);

#ifdef __cplusplus
}
#endif

#endif
