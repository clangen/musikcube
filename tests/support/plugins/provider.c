#include "probe.h"
#include <stdlib.h>
#include <string.h>

#ifdef INIT_FAIL
static const mc_string_view tag = { "failed-fixture", 14 };
#else
static const mc_string_view tag = { "fixture", 7 };
#endif

/* each initialized session owns its host pointer and table. the native image
can serve two independent hosts without mutable process-global engine state. */
typedef struct fixture_state {
    const mc_host_api* host;
    test_probe_api probe;
} fixture_state;

/* lifecycle diagnostics remain host-owned observations after native unload. */
static void note(const mc_host_api* host, const char* message) {
    mc_string_view text = { message, strlen(message) };
    host->log(host->host_context, MC_LOG_INFO, tag, text);
}

/* touch session-owned state as well as code, so a premature free is observable. */
static mc_result MC_CALL ping(void* context, int32_t* output) {
    const fixture_state* state = context;
    if (!state || !state->host || !output) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *output = 42;
    return MC_OK;
}

/* mutate a stack message after error creation to prove the host copied it. */
static mc_result MC_CALL make_error(void* context, mc_error** error) {
    const fixture_state* state = context;
    char message[] = "provider detail";
    mc_string_view view = { message, sizeof(message) - 1 };
    const mc_result result = state->host->create_error(state->host->host_context, MC_ERROR_PLUGIN, view, error);
    memset(message, '?', sizeof(message) - 1);
    return result;
}

/* the descriptor and ID may be temporary; the session's table stays alive. */
static mc_result MC_CALL register_again(void* context) {
    const fixture_state* state = context;
    char id[] = "probe";
    mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
    capability.id.data = id;
    capability.id.length = sizeof(id) - 1;
    capability.api = &state->probe;
    capability.api_size = sizeof(state->probe);
#ifdef BAD_CAPABILITY
    capability.api_size -= 1;
#endif
    const mc_result result = state->host->register_capability(state->host->host_context, &capability);
    memset(id, '?', sizeof(id) - 1);
    return result;
}

/* assign partial state before other fallible work; shutdown can then clean it
when registration or initialization fails. no allocation happens in entry. */
static mc_result MC_CALL initialize(const mc_host_api* host, void** context, mc_error** error) {
    fixture_state* state = calloc(1, sizeof(fixture_state));
    *context = state;
    if (!state) {
        return MC_ERROR_INTERNAL;
    }
    state->host = host;
    state->probe = (test_probe_api) { sizeof(test_probe_api), state, ping, register_again, make_error };
    note(host, "initialize");
    const mc_result registered = register_again(state);
    if (registered != MC_OK) {
        return registered;
    }
    if (register_again(state) != MC_ERROR_INVALID_ARGUMENT) {
        return MC_ERROR_INTERNAL;
    }
#ifdef INIT_FAIL
    make_error(state, error);
    return MC_ERROR_PLUGIN;
#else
    (void) error;
    return MC_OK;
#endif
}

/* null is valid if allocation failed; otherwise release even partial state. */
static void MC_CALL shutdown(void* context) {
    fixture_state* state = context;
    if (state) {
        note(state->host, "shutdown");
        free(state);
    }
}

/* all variants use only public C declarations and callback tables; none links
musikcore. rejected negotiation must never enter initialize or shutdown. */
#ifdef NO_ENTRY
MC_PLUGIN_API mc_result MC_CALL not_a_plugin(void) {
    return MC_OK;
}
#else
MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* output, mc_error** error)
{
    /* reject an unknown host major before reading its table layout. */
    if (MC_ABI_VERSION_MAJOR(version) != MC_ABI_VERSION_MAJOR(MC_PLUGIN_ABI_VERSION) ||
        MC_ABI_VERSION_MINOR(version) < MC_ABI_VERSION_MINOR(MC_PLUGIN_ABI_VERSION))
    {
        return MC_ERROR_UNSUPPORTED;
    }
    if (!host || host->struct_size < sizeof(mc_host_api) || !host->log || !host->create_error ||
        !host->destroy_error || !host->register_capability || !output ||
        output->struct_size < sizeof(mc_plugin_api) || version != host->abi_version)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    note(host, "negotiate");
    mc_plugin_capability unavailable = MC_PLUGIN_CAPABILITY_DEFAULT;
    if (host->register_capability(host->host_context, &unavailable) != MC_ERROR_IN_FLIGHT) {
        return MC_ERROR_INTERNAL;
    }
#ifdef ENTRY_FAIL
    const mc_string_view message = { "entry failure", 13 };
    host->create_error(host->host_context, MC_ERROR_PLUGIN, message, error);
    return MC_ERROR_PLUGIN;
#endif
    mc_plugin_api result = MC_PLUGIN_API_DEFAULT;
    result.id = (mc_string_view) { "org.musikcube.fixture", sizeof("org.musikcube.fixture") - 1 };
#ifdef INIT_FAIL
    result.id = (mc_string_view) { "org.musikcube.failure", sizeof("org.musikcube.failure") - 1 };
#endif
    /* escaped UTF-8 is independent of the compiler source encoding. */
    result.name = (mc_string_view) { "pure C fixture \xe2\x99\xab", sizeof("pure C fixture \xe2\x99\xab") - 1 };
    result.initialize = initialize;
    result.shutdown = shutdown;
#ifdef INCOMPATIBLE
    result.abi_version = MC_ABI_VERSION_ENCODE(99, 0);
#endif
#ifdef MISSING_VERSION
    result.abi_version = 0;
#endif
#ifdef NEWER_MINOR
    result.abi_version = MC_PLUGIN_ABI_VERSION + 1;
#endif
#ifdef TRUNCATED
    result.struct_size = (uint32_t) offsetof(mc_plugin_api, shutdown);
#endif
#ifdef MISSING_SHUTDOWN
    result.shutdown = NULL;
#endif
#ifdef BAD_ID
    result.id = (mc_string_view) { "bad/id", 6 };
#endif
#ifdef BAD_UTF8
    result.name = (mc_string_view) { "\xc0\x80", 2 };
#endif
    *output = result;
    (void) error;
    return MC_OK;
}
#endif
