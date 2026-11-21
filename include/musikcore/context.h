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
    MC_CONTEXT_MANAGED_RUNTIME = 1,
    MC_CONTEXT_ENABLE_LOGGING = 2
};

typedef struct mc_context_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Requested ABI version. Zero selects the current version. */
    uint32_t requested_abi_version;
    /* Managed plugin directory, copied at creation. Empty uses the host default.
       Set before plugin discovery. The directory is fixed for the process. */
    mc_string_view plugin_directory;
    /* MC_CONTEXT_MANAGED_RUNTIME starts playback, plugins, and the configured
       library in a production build. Zero creates a request dispatcher. */
    /* MC_CONTEXT_ENABLE_LOGGING enables diagnostics in the managed runtime. */
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
    /* Optional observer of managed runtime diagnostics. Zero disables delivery. */
    mc_log_fn log;
    /* Caller-owned observer argument. Keep it valid until shutdown returns. */
    void* log_user_data;
} mc_context_options;

#define MC_CONTEXT_OPTIONS_DEFAULT \
    { sizeof(mc_context_options), 0, { NULL, 0 }, 0, NULL, NULL, 0, 0, 0, NULL, NULL }

/* Null options use defaults. Callers own directory storage and may release it on return.
   out_context is required. out_error is optional; initialize both supplied slots
   to null. Occupied slots are rejected unchanged. A second context returns MC_ERROR_BUSY.
   A successful context is owned by the caller until destroy succeeds. */
MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error);

/* Select a caller-owned library for later submissions. Null clears it.
   Host-managed contexts use mc_context_select_library instead. Previously
   accepted requests and retained handles keep their original library. */
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
   discard queued notification. Keep user_data alive through callback delivery. */
MC_API mc_result MC_CALL mc_context_set_library_changed_callback(
    mc_context* context, mc_library_changed_fn callback, void* user_data);

/* Open an owned handle on the playback engine's thread. Releasing a handle does
   not stop playback. Contexts without an engine return MC_ERROR_UNSUPPORTED. */
MC_API mc_result MC_CALL mc_context_get_playback(const mc_context* context, mc_playback** out_playback);

/* For a managed runtime, create binds the dispatcher thread. Otherwise, the
   first dispatch or shutdown binds its thread. Ready callbacks run inline,
   outside internal locks. reentrant dispatch returns MC_ERROR_BUSY; other threads return MC_ERROR_WRONG_THREAD.
   Callbacks may submit/cancel requests, but cannot shut down or destroy the context.
   Rejected submissions set out_request to zero and never invoke the callback.
   Accepted submissions deliver exactly one completion, including on shutdown.
   Handles must remain valid for the duration of concurrent calls using them. */
MC_API mc_result MC_CALL mc_context_dispatch(mc_context *context);
/* Selects cancellation if no terminal result exists. A mutation already
   submitted to its backend returns MC_ERROR_IN_FLIGHT and still completes. */
MC_API mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id request);
/* Stops the managed runtime and invalidates its playback handles. Settles
   requests on the dispatcher thread and waits for context-owned work and wakeup
   hooks to stop. Caller-owned sessions have their own lifetime. */
MC_API mc_result MC_CALL mc_context_shutdown(mc_context *context);

/* Shuts down on the dispatcher thread and clears the slot on success. Null is accepted.
   If shutdown returns an error, the handle stays valid. Retry on the dispatcher thread outside callbacks. */
MC_API mc_result MC_CALL mc_context_destroy(mc_context **context);

#ifdef __cplusplus
}
#endif

#endif
