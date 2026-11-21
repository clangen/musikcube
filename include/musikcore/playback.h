#ifndef MUSIKCORE_PLAYBACK_H
#define MUSIKCORE_PLAYBACK_H

#include <musikcore/abi.h>
#include <musikcore/library.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Playback operations require the session's owner thread unless documented
   otherwise. Required handles and output
   slots are non-null. New queue output slots must be initialized to null and
   remain unchanged on failure. Destroy functions accept null slots and clear
   owned handles. Snapshot accessors accept null and return empty sentinels.
   Releasing a snapshot or handle never stops the playback session. */

typedef uint32_t mc_replay_gain_mode;
enum {
    MC_REPLAY_GAIN_DISABLED,
    MC_REPLAY_GAIN_TRACK,
    MC_REPLAY_GAIN_ALBUM
};

typedef struct mc_playback_gain_settings {
    /* Gain adjustment in decibels. Defaults to 0 dB. */
    double preamp_db;
    /* Replay gain normalization mode. Defaults to disabled. */
    mc_replay_gain_mode replay_gain;
} mc_playback_gain_settings;

/* Read configured gain settings. Changes apply to subsequent track loads. */
MC_API mc_result MC_CALL mc_playback_get_gain_settings(mc_playback_gain_settings* settings);
/* Persist a finite preamp adjustment between -20 and 20 dB. */
MC_API mc_result MC_CALL mc_playback_set_preamp(double decibels);
/* Persist a replay gain normalization mode. */
MC_API mc_result MC_CALL mc_playback_set_replay_gain(mc_replay_gain_mode mode);

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
    MC_PLAYBACK_SET_VOLUME,
    MC_PLAYBACK_RELOAD_OUTPUT
};

typedef uint32_t mc_playback_state;
enum {
    MC_PLAYBACK_STOPPED = 1,
    MC_PLAYBACK_PAUSED,
    MC_PLAYBACK_PREPARED,
    MC_PLAYBACK_PLAYING
};

typedef uint32_t mc_playback_stream_state;
enum {
    MC_PLAYBACK_STREAM_STOPPED = 0,
    MC_PLAYBACK_STREAM_BUFFERING,
    MC_PLAYBACK_STREAM_BUFFERED,
    MC_PLAYBACK_STREAM_PLAYING,
    MC_PLAYBACK_STREAM_ALMOST_DONE,
    MC_PLAYBACK_STREAM_FINISHED,
    MC_PLAYBACK_STREAM_DESTROYED,
    MC_PLAYBACK_STREAM_OPEN_FAILED
};

typedef uint32_t mc_playback_repeat_mode;
enum {
    MC_PLAYBACK_REPEAT_OFF = 0,
    MC_PLAYBACK_REPEAT_TRACK,
    MC_PLAYBACK_REPEAT_LIST
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
    /* Current stream state. */
    mc_playback_stream_state stream;
    /* Current repeat mode. */
    mc_playback_repeat_mode repeat;
    /* True when the queue is shuffled. */
    bool shuffled;
    /* True when the current track carries non-default replay gain or peak data. */
    bool has_replay_gain;
} mc_playback_snapshot;

#define MC_PLAYBACK_SNAPSHOT_DEFAULT { sizeof(mc_playback_snapshot), MC_PLAYBACK_STOPPED, 0, 0, 0, false, \
    MC_PLAYBACK_STREAM_STOPPED, MC_PLAYBACK_REPEAT_OFF, false, false }

/* Sample the session on its owner thread. On failure, snapshot remains unchanged. */
MC_API mc_result MC_CALL mc_playback_get_snapshot(const mc_playback *playback, mc_playback_snapshot *snapshot);

/* Return an owned reference to the active or buffering track in an empty output
   slot. Returns MC_ERROR_NOT_FOUND if the track or its origin is unavailable. */
MC_API mc_result MC_CALL mc_playback_get_current_track(const mc_playback* playback, mc_track_ref** out_track);

/* On the owner thread, copy the active or buffering track's loaded title, artist,
   album, and duration into an owned one-row page. Pass an empty output slot.
   No catalog I/O is performed. Returns MC_ERROR_NOT_FOUND when there is no
   active track or its origin is unavailable.
   Release the page with mc_track_page_destroy. */
MC_API mc_result MC_CALL mc_playback_get_current_track_metadata(const mc_playback* playback,
    mc_track_page** out_page);

typedef enum mc_playback_event {
    /* The active track changed or became unavailable. Stream progress has its own event. */
    MC_PLAYBACK_CHANGED_TRACK = 1,
    MC_PLAYBACK_CHANGED_STATE,
    MC_PLAYBACK_CHANGED_STREAM,
    MC_PLAYBACK_CHANGED_POSITION,
    MC_PLAYBACK_CHANGED_VOLUME,
    MC_PLAYBACK_CHANGED_MODE
} mc_playback_event;

typedef void (MC_CALL *mc_playback_changed_fn)(void *user_data, mc_playback_event event);
/* Register, replace, or unsubscribe from any thread. Pass NULL to unsubscribe.
   Each handle has its own subscription, independent of queue notifications.
   An empty event list subscribes to all kinds. A nonempty list is copied
   during registration. Repeated pending events of one kind may be coalesced.
   Each callback receives one event kind indicating which state to reread. Callbacks run
   outside engine locks on the owner thread and may call playback APIs.
   Replacement, unsubscription, and handle destruction discard queued delivery
   and wait for a callback running on another thread. A callback can unsubscribe
   itself. Keep user_data alive through any invocation that has already started. */
MC_API mc_result MC_CALL mc_playback_set_changed_callback(
    mc_playback *playback, mc_playback_changed_fn callback, void *user_data,
    const mc_playback_event *events, size_t event_count);

/* Apply a command to an existing playback session on its owner thread.
   Value is seconds for SET_POSITION, or a fraction from zero to one for SET_VOLUME.
   Other commands ignore value.
   Returns MC_ERROR_BUSY on another thread or MC_ERROR_NOT_FOUND after the session ends. */
MC_API mc_result MC_CALL mc_playback_control(mc_playback *playback, mc_playback_command command, double value);

/* Replace the queue with the ordered result and play from row on the owner thread.
   Returns MC_ERROR_NOT_FOUND if the row or its original library session is unavailable. */
MC_API mc_result MC_CALL mc_playback_play_result(mc_playback *playback, const mc_track_result *result, size_t row);

typedef uint32_t mc_playback_track_action;
enum {
    MC_PLAYBACK_TRACK_PLAY_NOW = 1,
    MC_PLAYBACK_TRACK_PLAY_NEXT,
    MC_PLAYBACK_TRACK_APPEND,
    MC_PLAYBACK_TRACK_PREPEND
};

/* Play now replaces the queue. Play next inserts after the retained position, or appends if absent.
   Call on the owner thread. The caller must keep track valid until the call returns.
   Returns MC_ERROR_NOT_FOUND if the originating library session is unavailable. */
MC_API mc_result MC_CALL mc_playback_use_track(
    mc_playback *playback, const mc_track_ref *track, mc_playback_track_action action);

typedef struct mc_play_queue mc_play_queue;

/* Capture the ordered queue on the owner thread. The caller owns the returned snapshot. */
MC_API mc_result MC_CALL mc_playback_get_queue(const mc_playback *playback, mc_play_queue **out_queue);
MC_API void MC_CALL mc_play_queue_destroy(mc_play_queue **queue);
/* Snapshot-owned track IDs, valid until snapshot destruction. Duplicates are preserved. */
MC_API const int64_t *MC_CALL mc_play_queue_get_ids(const mc_play_queue *queue, size_t *out_count);
/* Compare queue revisions. Position changes alone leave rows unchanged. */
MC_API bool MC_CALL mc_play_queue_same_rows(const mc_play_queue* queue, const mc_play_queue* previous);
/* Return an owned track result sharing this snapshot's ordered rows and catalog.
   Returns MC_ERROR_NOT_FOUND when no catalog is available. */
MC_API mc_result MC_CALL mc_play_queue_get_tracks(const mc_play_queue* queue, mc_track_result** result);
/* Returns SIZE_MAX when the queue has no current row. */
MC_API size_t MC_CALL mc_play_queue_get_current_row(const mc_play_queue *queue);
/* Returns the retained playback position, including after Stop, or SIZE_MAX. */
MC_API size_t MC_CALL mc_play_queue_get_retained_row(const mc_play_queue *queue);
/* Load an ordered result from the selected library on the owner thread without starting playback. */
MC_API mc_result MC_CALL mc_playback_replace_queue(mc_playback *playback, const mc_track_result *result);

/* Replace the queue while retaining active playback. Use row as a hint when locating the playing track. */
MC_API mc_result MC_CALL mc_playback_hot_swap_queue(mc_playback* playback, const mc_track_result* result, size_t row);
/* Insert the ordered result using prepend, append, or play next. Duplicates are preserved. */
MC_API mc_result MC_CALL mc_playback_insert_result(mc_playback* playback, const mc_track_result* result,
    mc_playback_track_action action);
MC_API bool MC_CALL mc_play_queue_is_shuffled(const mc_play_queue *queue);
/* Request playback of a row in the supplied snapshot on the owner thread.
   A stale snapshot returns MC_ERROR_STALE_SNAPSHOT. An unavailable row returns MC_ERROR_NOT_FOUND. */
MC_API mc_result MC_CALL mc_playback_play_queue_row(
    mc_playback *playback, const mc_play_queue *queue, size_t row);

typedef void (MC_CALL *mc_play_queue_changed_fn)(void *user_data);
/* Register on the owner thread. The dispatcher delivers changes outside the engine's signal locks.
   Pending notifications may be coalesced. Replacing the callback discards pending delivery.
   Pass NULL to unsubscribe. Keep user_data alive until unsubscription or handle destruction. */
MC_API mc_result MC_CALL mc_playback_set_queue_changed_callback(
    mc_playback *playback, mc_play_queue_changed_fn callback, void *user_data);

/* Edit zero-based positions from the supplied snapshot on the owner thread.
   A stale snapshot returns MC_ERROR_STALE_SNAPSHOT. An unavailable row returns MC_ERROR_NOT_FOUND. */
MC_API mc_result MC_CALL mc_playback_queue_move(
    mc_playback *playback, const mc_play_queue *queue, size_t from, size_t to);
MC_API mc_result MC_CALL mc_playback_queue_remove(
    mc_playback *playback, const mc_play_queue *queue, size_t row);
MC_API mc_result MC_CALL mc_playback_queue_clear(mc_playback *playback);
/* Release the handle. The playback session and its queue remain active. */
MC_API void MC_CALL mc_playback_destroy(mc_playback **playback);

#ifdef __cplusplus
}
#endif

#endif
