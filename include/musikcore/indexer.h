#ifndef MUSIKCORE_INDEXER_H
#define MUSIKCORE_INDEXER_H

#include <musikcore/context.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mc_indexer_scan;
enum {
    MC_INDEXER_SCAN_ALL,
    MC_INDEXER_SCAN_LOCAL,
    MC_INDEXER_REBUILD
};

typedef uint32_t mc_indexer_state;
enum {
    MC_INDEXER_IDLE,
    MC_INDEXER_INDEXING,
    MC_INDEXER_STOPPING,
    MC_INDEXER_STOPPED
};

typedef uint32_t mc_indexer_event;
enum {
    MC_INDEXER_STARTED,
    MC_INDEXER_PROGRESS,
    MC_INDEXER_FINISHED,
    MC_INDEXER_CANCELLED
};

typedef void (MC_CALL *mc_indexer_paths_fn)(void* user_data, const mc_string_view* paths, size_t count);
typedef void (MC_CALL *mc_indexer_event_fn)(void* user_data, mc_indexer_event event, int updates);

/* Operations target the host's local catalog even when a remote catalog is
   selected. Contexts without a local indexer return MC_ERROR_UNSUPPORTED. */
/* use the managed context dispatcher thread for indexer access. other threads
   return MC_ERROR_WRONG_THREAD; a stopped runtime returns MC_ERROR_BUSY. */
MC_API mc_result MC_CALL mc_indexer_get_state(const mc_context* context, mc_indexer_state* state);
/* Deliver an inline snapshot. Path views are valid only during the callback. */
MC_API mc_result MC_CALL mc_indexer_read_paths(
    const mc_context* context, mc_indexer_paths_fn read, void* user_data);
MC_API mc_result MC_CALL mc_indexer_add_path(mc_context* context, mc_string_view path);
MC_API mc_result MC_CALL mc_indexer_remove_path(mc_context* context, mc_string_view path);
MC_API mc_result MC_CALL mc_indexer_schedule(mc_context* context, mc_indexer_scan scan);
/* request cooperative cancellation of the active scan and discard scans queued
   before this call. returns without waiting for the scan to finish. cancellation
   is an idle no-op, retains committed changes, and does not stop the
   indexer. an active cancelled scan emits MC_INDEXER_CANCELLED instead of
   MC_INDEXER_FINISHED after its workers settle. later scheduled scans are kept.
   cancellation may wait for a provider lifecycle callback, but does not
   join scan workers or interrupt an in-progress tag-reader/filesystem call.
   cancelled scans skip remaining cleanup, so unvisited tracks are retained.
   updates reports the number of items processed, as for progress/finish events.
   state becomes MC_INDEXER_IDLE before terminal publication; later work may
   already have started when the dispatcher delivers that event. */
MC_API mc_result MC_CALL mc_indexer_cancel(mc_context* context);
/* Register on the dispatcher thread. Consecutive progress events may be
   coalesced. Pass NULL to unsubscribe and discard pending events. */
MC_API mc_result MC_CALL mc_indexer_set_callback(mc_context* context, mc_indexer_event_fn callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif
