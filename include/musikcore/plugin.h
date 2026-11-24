#ifndef MUSIKCORE_PLUGIN_H
#define MUSIKCORE_PLUGIN_H

#include <musikcore/abi.h>
#include <musikcore/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/* provider and application ABI versions are separate contracts. neither is
frozen during development. plugins must not link to private core interfaces. */
#define MC_PLUGIN_ABI_VERSION MC_ABI_VERSION_ENCODE(0, 1)

/* byte limits exclude a terminating NUL. views may not contain embedded NULs. */
enum {
    MC_PLUGIN_ID_MAX_BYTES = 256,
    MC_PLUGIN_METADATA_MAX_BYTES = 4096,
    MC_PLUGIN_DIAGNOSTIC_MAX_BYTES = 1024 * 1024
};

/* a capability identifies a typed factory table. the host copies the descriptor
and ID during registration; the immutable table and anything it references stay
plugin-owned until shutdown. each table begins with its uint32_t byte size.
identifiers are limited to MC_PLUGIN_ID_MAX_BYTES. registration is
owner-thread-only during initialize and becomes visible only if initialization
succeeds. typed factory contracts use the negotiated provider ABI version. */
typedef struct mc_plugin_capability {
    /* bytes available in this descriptor; initialize with the default below. */
    uint32_t struct_size;
    /* nonempty ASCII identifier, using letters, digits, '.', '_', or '-'. */
    mc_string_view id;
    /* immutable, suitably aligned table; null is invalid when registering. */
    const void* api;
    /* readable table bytes, matching its leading struct_size; default zero. */
    uint32_t api_size;
} mc_plugin_capability;

#define MC_PLUGIN_CAPABILITY_DEFAULT { sizeof(mc_plugin_capability), { NULL, 0 }, NULL, 0 }

/* provided by the core, not imported symbols. the table/context remain valid
from entry until shutdown returns. log/error functions are bootstrap-safe and
thread-safe; registration is allowed only on the initialization thread. plugins
must join their workers before returning from shutdown. all base callbacks are
required and must not throw. text and descriptor arguments are consumed during
the call; registered tables follow the capability lifetime above. log tags use
the metadata byte limit; messages use MC_PLUGIN_DIAGNOSTIC_MAX_BYTES. invalid log views are
ignored; create_error rejects them. these are not real-time APIs. */
typedef struct mc_host_api {
    /* readable table bytes; check before accessing a trailing field. */
    uint32_t struct_size;
    /* provider ABI supported by this host; default MC_PLUGIN_ABI_VERSION. */
    uint32_t abi_version;
    /* opaque core-owned callback state; pass unchanged to every host call. */
    void* host_context;
    /* copies/consumes text during the call; does not retain either view. */
    void (MC_CALL* log)(void* host_context, mc_log_level level, mc_string_view component, mc_string_view message);
    /* creates a core-owned error; returns MC_OK on creation, not 'code'.
    the empty output slot receives ownership; code must be a failure status. */
    mc_result (MC_CALL* create_error)(void* host_context, mc_result code, mc_string_view message, mc_error** out_error);
    /* destroys a host-created error and clears its slot; null is accepted. */
    void (MC_CALL* destroy_error)(void* host_context, mc_error** error);
    /* copies registration metadata; duplicate IDs are rejected. no factory
    becomes callable before successful initialize. there is no implicit replace.
    returns MC_ERROR_WRONG_THREAD off-owner,
    MC_ERROR_IN_FLIGHT outside initialize, or MC_ERROR_INVALID_ARGUMENT for a bad
    descriptor/duplicate ID. */
    mc_result (MC_CALL* register_plugin_capability)(void* host_context, const mc_plugin_capability* capability);
} mc_host_api;

#define MC_HOST_API_DEFAULT { sizeof(mc_host_api), MC_PLUGIN_ABI_VERSION, NULL, NULL, NULL, NULL, NULL }

/* metadata views are UTF-8, limited to MC_PLUGIN_METADATA_MAX_BYTES (IDs use
MC_PLUGIN_ID_MAX_BYTES), and refer to immutable plugin storage through shutdown.
initialize/shutdown run on the loading thread. initialize owns all persistent
allocation; entry must leave no state needing cleanup if negotiation fails.
once initialize is entered, shutdown is called exactly once, even on failure;
shutdown must safely clean partially initialized state. plugins stay loaded
for the engine session. before shutdown/unload, the host stops plugin users,
finishes their calls/callbacks, and destroys all instances. */
typedef struct mc_plugin_api {
    /* entry receives host capacity here; return bytes actually initialized,
    never writing beyond that capacity. default sizeof(mc_plugin_api). */
    uint32_t struct_size;
    /* minimum provider ABI required; nonzero, same major, minor no newer
    than host. the default initializer uses MC_PLUGIN_ABI_VERSION. */
    uint32_t abi_version;
    /* nonempty stable ASCII plugin ID; same alphabet as capability IDs. */
    mc_string_view id;
    /* UTF-8 display name; nonempty. default empty until entry fills it. */
    mc_string_view name;
    /* UTF-8 version text; optional, default empty. */
    mc_string_view version;
    /* UTF-8 author text; optional, default empty. */
    mc_string_view author;
    /* initialize and register capabilities. out_plugin_context points to a
    null slot: assign plugin-owned state before any further fallible work.
    optional error transfers ownership to the host. required, default null. */
    mc_result (MC_CALL* initialize)(const mc_host_api* host, void** out_plugin_context, mc_error** out_error);
    /* called once with the state returned by initialize, including on failure.
    accept null for failed allocation; release state and join workers here.
    must not throw: the host terminates before unloading if cleanup throws. */
    void (MC_CALL* shutdown)(void* plugin_context);
} mc_plugin_api;

#define MC_PLUGIN_API_DEFAULT { sizeof(mc_plugin_api), MC_PLUGIN_ABI_VERSION, \
    { NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }, NULL, NULL }

/* validate the host ABI/callbacks and the caller-provided plugin-table capacity
before mc_plugin_get_api reads host callbacks or writes plugin metadata. newer
host minor versions are accepted when they supply this SDK's required prefix/callbacks. no allocation,
host calls, or plugin-table mutation. out_error is optional, points to an empty
slot, and is cleared. after success, assign MC_PLUGIN_API_DEFAULT to out_plugin
before filling metadata/hooks; validation does not initialize the table. */
static inline mc_result mc_plugin_validate_host_api(
    uint32_t host_abi_version, const mc_host_api* host, mc_plugin_api* out_plugin, mc_error** out_error)
{
    if (out_error) {
        *out_error = NULL;
    }
    if (MC_ABI_VERSION_MAJOR(host_abi_version) != MC_ABI_VERSION_MAJOR(MC_PLUGIN_ABI_VERSION) ||
        MC_ABI_VERSION_MINOR(host_abi_version) < MC_ABI_VERSION_MINOR(MC_PLUGIN_ABI_VERSION))
    {
        return MC_ERROR_UNSUPPORTED;
    }
    if (!host || host->struct_size < sizeof(*host) || host->abi_version != host_abi_version ||
        !host->log || !host->create_error || !host->destroy_error || !host->register_plugin_capability ||
        !out_plugin || out_plugin->struct_size < sizeof(*out_plugin))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return MC_OK;
}

typedef mc_result (MC_CALL* mc_plugin_get_api_fn)(
    uint32_t host_abi_version, const mc_host_api* host, mc_plugin_api* out_plugin, mc_error** out_error);

/* define MC_BUILDING_PLUGIN when compiling the provider.
the only provider export. only host log/error functions may be used here.
on failure, out_error may receive a host-created error. the host validates all
required fields and ABI compatibility before invoking initialize. */
#if defined(MC_BUILDING_PLUGIN)
MC_PLUGIN_API
#endif
mc_result MC_CALL mc_plugin_get_api(
    uint32_t host_abi_version, const mc_host_api* host, mc_plugin_api* out_plugin, mc_error** out_error);

#ifdef __cplusplus
}
#endif
#endif
