#ifndef MUSIKCORE_MUSIKCORE_H
#define MUSIKCORE_MUSIKCORE_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_context mc_context;
typedef uint64_t mc_request_id;
typedef void (MC_CALL *mc_wakeup_fn)(void *user_data);

typedef struct mc_context_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Requested ABI version. Zero selects the current version. */
    uint32_t requested_abi_version;
    /* UTF-8 data root, copied at creation. An empty view selects the default root. */
    mc_string_view data_directory;
    /* UTF-8 plugin root, copied at creation. An empty view selects the default root. */
    mc_string_view plugin_directory;
    /* Reserved for context options. Must be zero. */
    uint32_t flags;
    /* Optional hook that signals the host loop. May run on a worker thread. */
    mc_wakeup_fn wakeup;
    /* Borrowed hook argument. Keep it valid until shutdown returns. */
    void *wakeup_user_data;
} mc_context_options;

#define MC_CONTEXT_OPTIONS_DEFAULT { sizeof(mc_context_options), 0, { NULL, 0 }, { NULL, 0 }, 0, NULL, NULL }

/* Null options use defaults. Directory views are copied during creation. */
/* Output slots must not already own handles. A second context returns MC_ERROR_BUSY. */
MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error);

/* The first dispatch binds its thread. Callbacks run inline for work already ready. */
MC_API mc_result MC_CALL mc_context_dispatch(mc_context *context);
/* Selects cancellation if the request still has no terminal result. */
MC_API mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id request);
/* Settles requests on the dispatcher thread. Waits for work and wakeup hooks to stop. */
MC_API mc_result MC_CALL mc_context_shutdown(mc_context *context);

/* Consumes the context and clears the slot. Null is accepted. */
MC_API void MC_CALL mc_context_destroy(mc_context **context);

#ifdef __cplusplus
}
#endif

#endif
