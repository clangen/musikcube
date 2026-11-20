#ifndef MUSIKCORE_CONTEXT_H
#define MUSIKCORE_CONTEXT_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_context mc_context;
typedef uint64_t mc_request_id;
/* Signal the host loop promptly. May run on a worker or submitting thread.
   Do not dispatch, shut down, or destroy the context from this hook. */
typedef void (MC_CALL *mc_wakeup_fn)(void *user_data);

enum {
    MC_TRACK_QUERY_DEFAULT_LIMIT = 100,
    MC_TRACK_QUERY_MAX_LIMIT = 1000,
    MC_TRACK_QUERY_MAX_FIELDS = 64
};

typedef struct mc_context_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Requested ABI version. Zero selects the current version. */
    uint32_t requested_abi_version;
    /* Reserved UTF-8 plugin root, copied at creation. The standalone ABI does not load plugins yet. */
    mc_string_view plugin_directory;
    /* Reserved for context options. Must be zero. */
    uint32_t flags;
    /* Optional hook that signals the host loop. May run on a worker thread. */
    mc_wakeup_fn wakeup;
    /* Caller-owned hook argument. Keep it valid until shutdown returns. */
    void *wakeup_user_data;
    /* Default query row limit, capped by the maximum. Nonpositive uses MC_TRACK_QUERY_DEFAULT_LIMIT. */
    int32_t track_query_default_limit;
    /* Maximum query row limit. Nonpositive uses MC_TRACK_QUERY_MAX_LIMIT. */
    int32_t track_query_max_limit;
    /* Maximum metadata field count. Nonpositive uses MC_TRACK_QUERY_MAX_FIELDS. */
    int32_t track_query_max_fields;
} mc_context_options;

#define MC_CONTEXT_OPTIONS_DEFAULT \
    { sizeof(mc_context_options), 0, { NULL, 0 }, 0, NULL, NULL, 0, 0, 0 }

/* Null options use defaults. Callers own directory storage and may release it on return. */
/* out_context is required. out_error is optional; initialize both supplied slots
   to null. Occupied slots are rejected unchanged. A second context returns MC_ERROR_BUSY.
   A successful context is owned by the caller until destroy succeeds. */
MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error);

/* The first dispatch or shutdown binds its thread. Ready callbacks run inline,
   outside internal locks. Nested/concurrent dispatch returns MC_ERROR_BUSY.
   Callbacks may submit/cancel requests, but cannot shut down or destroy the context.
   Rejected submissions set out_request to zero and never invoke the callback.
   Accepted submissions deliver exactly one completion, including on shutdown.
   Handles must remain valid for the duration of concurrent calls using them. */
MC_API mc_result MC_CALL mc_context_dispatch(mc_context *context);
/* Selects cancellation if the request still has no terminal result. */
MC_API mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id request);
/* Settles requests on the dispatcher thread. Waits for work and wakeup hooks to stop. */
MC_API mc_result MC_CALL mc_context_shutdown(mc_context *context);

/* Shuts down on the dispatcher thread and clears the slot on success. Null is accepted. */
/* If shutdown is busy, the handle stays valid. Retry on the dispatcher thread outside callbacks. */
MC_API void MC_CALL mc_context_destroy(mc_context **context);

#ifdef __cplusplus
}
#endif

#endif
