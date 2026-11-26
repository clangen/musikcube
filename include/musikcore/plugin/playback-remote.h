#ifndef MUSIKCORE_PLUGIN_PLAYBACK_REMOTE_H
#define MUSIKCORE_PLUGIN_PLAYBACK_REMOTE_H

#include <musikcore/playback.h>
#include <musikcore/indexer.h>
#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_PLAYBACK_REMOTE "org.musikcube.playback_remote"

/* maximum commands awaiting dispatch for one remote. */
enum { MC_PLAYBACK_REMOTE_MAX_PENDING_COMMANDS = 64 };

/* reports a command's terminal result on the playback owner thread. do not
wait here for another owner-thread completion. */
typedef void (MC_CALL* mc_playback_remote_completion_fn)(void* user_data, mc_result result);

/* receives an owned playback handle on the owner thread. destroy it when no
longer needed. a cancelled request supplies a null handle. do not block here. */
typedef void (MC_CALL* mc_playback_remote_access_fn)(void* user_data, mc_result result, mc_playback* playback);

/* receives state during a synchronous owner-thread read. do not wait here for
another owner-thread completion. */
typedef void (MC_CALL* mc_playback_remote_state_fn)(
    void* user_data, const mc_playback_snapshot* snapshot, const mc_track_ref* track);

/* lets a remote request playback commands from its own workers. */
struct mc_plugin_playback_service {
    /* available bytes. initialize with MC_PLUGIN_PLAYBACK_SERVICE_DEFAULT. */
    uint32_t struct_size;

    /* host-owned state; pass unchanged to each operation. */
    void* context;

    /* read the initial state on the owner thread. the views expire when read
    returns; worker threads receive MC_ERROR_WRONG_THREAD. */
    mc_result (MC_CALL* read_state)(void* context, mc_playback_remote_state_fn read, void* user_data);

    /* enqueue after start begins and before stop, from any thread, without
    calling completion inline. before start or during shutdown, returns
    MC_ERROR_CANCELLED without a callback. the owner
    may complete a worker's request before this call returns; user_data must
    already be ready and remain valid through the callback. accepted commands
    complete once, including MC_ERROR_CANCELLED during shutdown. rejected
    commands have no callback. a full queue returns MC_ERROR_QUEUE_FULL.
    set-position uses seconds; set-volume uses 0 to 1. */
    mc_result (MC_CALL* control_async)(void* context, mc_playback_command command, double value,
        mc_playback_remote_completion_fn completion, void* user_data);

    /* request an owned handle for using the normal playback and queue APIs on
    the owner thread. accepted requests complete once, including at shutdown;
    the callback may run before a submitting worker returns. keep user_data
    alive until completion. rejected requests have no callback. the command
    queue is shared with control_async and may return MC_ERROR_QUEUE_FULL. */
    mc_result (MC_CALL* with_playback_async)(void* context, mc_playback_remote_access_fn completion, void* user_data);
};

#define MC_PLUGIN_PLAYBACK_SERVICE_DEFAULT { sizeof(mc_plugin_playback_service), NULL, NULL, NULL, NULL }

/* describes one playback change delivered to a remote. */
typedef struct mc_plugin_playback_event {
    /* available bytes. initialize with MC_PLUGIN_PLAYBACK_EVENT_DEFAULT. */
    uint32_t struct_size;

    /* the changed part of playback; defaults to MC_PLAYBACK_CHANGED_TRACK. */
    mc_playback_event event;

    /* playback state at the time of delivery. */
    mc_playback_snapshot playback;

    /* current track on track changes, otherwise null. valid only during the callback. */
    const mc_track_ref* track;

} mc_plugin_playback_event;

#define MC_PLUGIN_PLAYBACK_EVENT_DEFAULT \
    { sizeof(mc_plugin_playback_event), MC_PLAYBACK_CHANGED_TRACK, MC_PLAYBACK_SNAPSHOT_DEFAULT, NULL }

/* receives playback changes for an integration such as a server or media keys.
the host stops and joins any workers before releasing its services. */
typedef struct mc_playback_remote_vtable {
    /* available bytes. initialize with MC_PLAYBACK_REMOTE_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* destroy the instance after its workers and callbacks have stopped. */
    void (MC_CALL* destroy)(void* instance);

    /* deliver on the owner thread; the event expires on return. do not wait
    here for another owner-thread completion. */
    void (MC_CALL* changed)(void* instance, const mc_plugin_playback_event* event);

    /* notify on the owner thread when queue contents or order change. do not
    wait here for another owner-thread completion. */
    void (MC_CALL* queue_changed)(void* instance);

    /* start on the owner thread after playback is ready. never wait here for
    an owner-thread completion. failure must still allow stop and join. */
    mc_result (MC_CALL* start)(void* instance, mc_error** error);

    /* prevent new work, detach external callbacks, and wake workers without
    relying on other remotes stopping. the host cancels this remote's accepted
    commands before join. do not wait for workers here. */
    void (MC_CALL* request_stop)(void* instance);

    /* wait for workers and callbacks to finish; none may enter after return. */
    mc_result (MC_CALL* join)(void* instance, mc_error** error);
} mc_playback_remote_vtable;

#define MC_PLAYBACK_REMOTE_VTABLE_DEFAULT { sizeof(mc_playback_remote_vtable), NULL, NULL, NULL, NULL, NULL, NULL }

/* creates a playback remote when the playback service is available. */
typedef struct mc_playback_remote_factory {
    /* available bytes. initialize with MC_PLAYBACK_REMOTE_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned factory state, or null. */
    void* context;

    /* immutable operations, valid until the instance is destroyed. */
    const mc_playback_remote_vtable* instance_api;

    /* create an inert instance in an empty slot; failure leaves the slot empty.
    do not start workers or callbacks until start is called. */
    mc_result (MC_CALL* create)(
        void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);
} mc_playback_remote_factory;

#define MC_PLAYBACK_REMOTE_FACTORY_DEFAULT { sizeof(mc_playback_remote_factory), NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
