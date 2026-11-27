#ifndef MUSIKCORE_PLAYER_H
#define MUSIKCORE_PLAYER_H

#include <musikcore/context.h>

#ifdef __cplusplus
extern "C" {
#endif

/* an explicitly owned URI player, independent of catalogs and indexing. all
operations use the context's creating thread. requires MC_SERVICE_PLUGINS;
creating a player does not change the context's selected services. */
typedef struct mc_player mc_player;
typedef uint32_t mc_player_state;
enum {
    MC_PLAYER_STOPPED,
    MC_PLAYER_BUFFERING,
    MC_PLAYER_PLAYING,
    MC_PLAYER_PAUSED,
    MC_PLAYER_FINISHED,
    MC_PLAYER_FAILED
};

typedef struct mc_player_snapshot {
    /* size in bytes; initialize with MC_PLAYER_SNAPSHOT_DEFAULT. */
    uint32_t struct_size;
    /* latest playback/open state; defaults to stopped. */
    mc_player_state state;
    /* current or pending seek position, in seconds; defaults to zero. */
    double position;
    /* duration in seconds; nonpositive means unknown, default -1. */
    double duration;
    /* normalized output volume in [0,1]; defaults to one. */
    double volume;
} mc_player_snapshot;
#define MC_PLAYER_SNAPSHOT_DEFAULT { sizeof(mc_player_snapshot), MC_PLAYER_STOPPED, 0, -1, 1 }

/* runs only during context dispatch. snapshot is callback-scoped. changes may
coalesce; this reports current state rather than a lossless event history.
user_data must remain alive until replacement, removal, or shutdown returns. */
typedef void (MC_CALL *mc_player_changed_fn)(void* user_data, const mc_player_snapshot* snapshot);

/* out_player must be an empty owned slot. providers/output settings remain
process-wide; each player owns its output. mixing depends on the backend. */
MC_API mc_result MC_CALL mc_player_create(mc_context* context, mc_player** out_player);
/* copy a nonempty UTF-8 path or provider URI before returning. supports local
file:// URIs with empty/localhost authority and percent-encoded paths. other
schemes depend on installed providers. internal musikcore: catalog URIs are
rejected. MC_OK accepts an asynchronous open; failures surface as FAILED. */
MC_API mc_result MC_CALL mc_player_play_uri(mc_player* player, mc_string_view uri);
/* pause and resume propagate device errors. MC_ERROR_IN_FLIGHT means an
output write or drain is still busy; the caller can retry or stop playback. */
MC_API mc_result MC_CALL mc_player_pause(mc_player* player);
MC_API mc_result MC_CALL mc_player_resume(mc_player* player);
MC_API mc_result MC_CALL mc_player_stop(mc_player* player);
/* finite nonnegative seconds; BUSY while buffering, NOT_FOUND with no active
track. seeking resumes playback,
including from pause. seek support depends on the source/decoder. */
MC_API mc_result MC_CALL mc_player_seek(mc_player* player, double seconds);
/* finite normalized volume in [0,1], independent of other player instances. */
MC_API mc_result MC_CALL mc_player_set_volume(mc_player* player, double volume);
/* snapshot remains unchanged on failure. */
MC_API mc_result MC_CALL mc_player_get_snapshot(const mc_player* player, mc_player_snapshot* snapshot);
/* NULL removes the observer; replacement discards notifications for the old one. */
MC_API mc_result MC_CALL mc_player_set_callback(mc_player* player, mc_player_changed_fn callback, void* user_data);
/* stops and waits for this player's workers, clears the owned slot on success.
NULL is accepted. called from its callback, returns BUSY and preserves the slot.
context shutdown stops all child players; retained handles return NOT_FOUND for
operations and remain destroyable on their owner thread. */
MC_API mc_result MC_CALL mc_player_destroy(mc_player** player);

#ifdef __cplusplus
}
#endif
#endif
