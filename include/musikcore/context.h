#ifndef MUSIKCORE_CONTEXT_H
#define MUSIKCORE_CONTEXT_H

#include <musikcore/abi.h>
#include <musikcore/log.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_context mc_context;
typedef struct mc_library mc_library;
typedef struct mc_playback mc_playback;
typedef uint64_t mc_request_id;

typedef uint32_t mc_library_kind;
enum {
    MC_LIBRARY_LOCAL = 0,
    MC_LIBRARY_REMOTE = 1,
    MC_LIBRARY_NONE = 2
};

typedef uint32_t mc_library_connection_state;
enum {
    MC_LIBRARY_DISCONNECTED = 0,
    MC_LIBRARY_CONNECTING,
    MC_LIBRARY_CONNECTED,
    MC_LIBRARY_AUTHENTICATION_FAILED
};

typedef uint32_t mc_library_connection_error;
enum {
    MC_LIBRARY_CONNECTION_OK = 0,
    MC_LIBRARY_CONNECTION_FAILED,
    MC_LIBRARY_CONNECTION_CLOSED,
    MC_LIBRARY_CONNECTION_INVALID_PASSWORD,
    MC_LIBRARY_CONNECTION_INCOMPATIBLE_VERSION
};

typedef struct mc_library_status {
    /* Kind of the selected catalog, including while disconnected. */
    mc_library_kind kind;
    /* Current connectivity of the selected catalog. */
    mc_library_connection_state state;
    /* Last remote connection error. */
    mc_library_connection_error error;
    /* Whether the selected catalog is configured. */
    bool configured;
    /* Callback-scoped remote endpoint. Empty for a local catalog. */
    mc_string_view endpoint;
    /* Callback-scoped version reported by the remote server. */
    mc_string_view server_version;
} mc_library_status;

typedef void (MC_CALL *mc_library_status_fn)(void *user_data, const mc_library_status *status);
typedef void (MC_CALL *mc_library_changed_fn)(void *user_data, bool selection_changed);
/* Signal the host loop promptly. May run on a worker or submitting thread.
   Do not dispatch, shut down, or destroy the context from this hook. */
typedef void (MC_CALL *mc_wakeup_fn)(void *user_data);

enum {
    MC_TRACK_QUERY_DEFAULT_LIMIT = 100,
    MC_TRACK_QUERY_MAX_LIMIT = 1000,
    MC_TRACK_QUERY_MAX_FIELDS = 64
};

enum {
    MC_CONTEXT_ENABLE_LOGGING = 2,
    /* start with the local catalog without writing the saved selection or
    connecting the default remote. selecting remote/reconnecting opts in. */
    MC_CONTEXT_START_LOCAL = 4
};

/* services are selected once at creation. prerequisites are added automatically:
library and plugins have none; indexing and playback each require library
and plugins. playback does not require indexing. */
typedef uint32_t mc_services;
enum {
    MC_SERVICE_NONE = 0,
    MC_SERVICE_LIBRARY = 1,
    /* provider session/environment; catalog bindings require MC_SERVICE_LIBRARY. */
    MC_SERVICE_PLUGINS = 2,
    MC_SERVICE_INDEXER = 4,
    MC_SERVICE_PLAYBACK = 8,
    MC_SERVICE_ALL = MC_SERVICE_LIBRARY | MC_SERVICE_PLUGINS | MC_SERVICE_INDEXER | MC_SERVICE_PLAYBACK
};

typedef struct mc_context_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Requested ABI version. Zero selects the current version. */
    uint32_t requested_abi_version;
    /* provider directory, copied at creation; used when MC_SERVICE_PLUGINS is
    selected. discovery fixes it for the process; empty reuses that directory,
    or selects the host default on first discovery. */
    mc_string_view plugin_directory;
    /* optional MC_CONTEXT_* flags; zero uses normal startup. logging requires
    at least one service; START_LOCAL requires MC_SERVICE_LIBRARY after dependency expansion. */
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
    /* optional observer of context runtime diagnostics. null disables delivery. */
    mc_log_fn log;
    /* Caller-owned observer argument. Keep it valid until shutdown returns. */
    void* log_user_data;
    /* requested MC_SERVICE_* bits, fixed for this context's lifetime. defaults
    to MC_SERVICE_ALL when options or this field are absent. explicitly use
    MC_SERVICE_NONE for only request processing with caller-owned libraries or
    remote sessions. a zero-filled full struct therefore requests no services. */
    mc_services services;
} mc_context_options;

#define MC_CONTEXT_OPTIONS_DEFAULT \
    { sizeof(mc_context_options), 0, { NULL, 0 }, 0, NULL, NULL, 0, 0, 0, NULL, NULL, MC_SERVICE_ALL }

/* Null options use defaults. Callers own directory storage and may release it on return.
   out_context is required. out_error is optional; initialize both supplied slots
   to null. Occupied slots are rejected unchanged. A second context returns MC_ERROR_BUSY.
   A successful context is owned by the caller until destroy succeeds. */
MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error);

/* read the immutable service set, including automatic prerequisites. remains
available after shutdown; it describes configuration rather than running state.
safe on any thread while context remains alive. out_services is required. */
MC_API mc_result MC_CALL mc_context_get_services(const mc_context* context, mc_services* out_services);

/* Select a caller-owned library for later submissions. A managed context also
   switches its playback service on the dispatcher thread. It accepts a catalog
   from the runtime or an explicitly connected remote session and rejects null.
   Selecting a host-configured catalog again uses mc_context_select_library.
   Other contexts accept null to clear the selection. Previously accepted
   requests and retained handles keep their original library. */
MC_API mc_result MC_CALL mc_context_set_library(mc_context* context, const mc_library* library);
/* Return an owned library in an empty output slot. An empty selection returns MC_ERROR_NOT_FOUND. */
MC_API mc_result MC_CALL mc_context_get_library(const mc_context* context, mc_library** out_library);

/* Select a host-configured catalog on the dispatcher thread. Contexts without
   host-configured catalogs return MC_ERROR_UNSUPPORTED. */
MC_API mc_result MC_CALL mc_context_select_library(mc_context* context, mc_library_kind kind);
/* Reconnect the host-configured remote catalog using its current settings. */
MC_API mc_result MC_CALL mc_context_reconnect_remote_library(mc_context* context);
/* Read a snapshot inline. The status and its strings are valid through the call. */
MC_API mc_result MC_CALL mc_context_read_library_status(
    const mc_context* context, mc_library_status_fn read, void* user_data);
/* Register on the dispatcher thread. Deliver changes on dispatch before request
   completions. Changes may be coalesced. Pass NULL to remove the observer and
   discard queued notification. The observer may remove itself. Keep user_data
   alive through callback delivery. */
MC_API mc_result MC_CALL mc_context_set_library_changed_callback(
    mc_context* context, mc_library_changed_fn callback, void* user_data);

/* Open an owned handle on the playback engine's thread. Releasing a handle does
   not stop playback. contexts without MC_SERVICE_PLAYBACK return MC_ERROR_UNSUPPORTED. */
MC_API mc_result MC_CALL mc_context_get_playback(const mc_context* context, mc_playback** out_playback);

/* When services are selected, create binds the dispatcher thread. Otherwise, the
   first dispatch, remote observer registration, or shutdown binds its thread. Ready callbacks run inline,
   outside internal locks. reentrant dispatch returns MC_ERROR_BUSY. dispatch
   from another thread after binding returns MC_ERROR_WRONG_THREAD.
   callbacks may submit or cancel requests. shutdown and destruction from a
   callback return MC_ERROR_BUSY.
   Rejected submissions set out_request to zero and never invoke the callback.
   Accepted submissions deliver exactly one completion, including on shutdown.
   Handles must remain valid for the duration of concurrent calls using them. */
MC_API mc_result MC_CALL mc_context_dispatch(mc_context *context);
/* Selects cancellation if no terminal result exists. A mutation already
   submitted to its backend returns MC_ERROR_IN_FLIGHT and still completes. */
MC_API mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id request);
/* use on the dispatcher thread outside callbacks. returns MC_ERROR_BUSY
   during reentrant dispatch and MC_ERROR_WRONG_THREAD on another thread after
   binding. stops the managed runtime, invalidates its playback handles,
   settles requests, and waits for context-owned work and wakeup hooks to stop.
   caller-owned sessions have their own lifetime. */
MC_API mc_result MC_CALL mc_context_shutdown(mc_context *context);

/* Shuts down on the dispatcher thread and clears the slot on success. Null is accepted.
   If shutdown returns an error, the handle stays valid. Retry on the dispatcher thread outside callbacks. */
MC_API mc_result MC_CALL mc_context_destroy(mc_context **context);

#ifdef __cplusplus
}
#endif

#endif
