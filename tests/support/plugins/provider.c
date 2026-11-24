#include "probe.h"
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/decoder.h>
#include <musikcore/plugin/encoder.h>
#include <musikcore/plugin/dsp.h>
#include <musikcore/plugin/settings.h>
#include <musikcore/plugin/indexer.h>
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
    const mc_result result = state->host->register_plugin_capability(state->host->host_context, &capability);
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
#if defined(BAD_OUTPUT) || defined(BAD_STREAM) || defined(BAD_DECODER) || defined(BAD_CONFIGURATION) || defined(BAD_ENCODER) || defined(BAD_DSP) || defined(BAD_INDEXER) || defined(BAD_NUMBER_CONFIGURATION) || defined(DUPLICATE_CONFIGURATION)
    /* registration must reject a sized factory with missing operations before
    playback can obtain a typed view of the provider. */
#if defined(BAD_CONFIGURATION)
    /* HTTP-style settings must have a valid fallback even when no file exists. */
    static const mc_integer_setting field = {
        .key = { "count", 5 },
        .default_value = 0,
        .minimum = 1,
        .maximum = 2,
    };
    static const mc_configuration_api factory = {
        .struct_size = sizeof(mc_configuration_api),
        .integers = &field,
        .integer_count = 1,
    };
    static const char id[] = MC_PLUGIN_CAPABILITY_CONFIGURATION;
#elif defined(BAD_NUMBER_CONFIGURATION)
    static const mc_number_setting field = {
        .key = { "number", 6 }, .default_value = 2, .minimum = 3, .maximum = 4, .precision = 1,
    };
    static const mc_configuration_api factory = {
        .struct_size = sizeof(mc_configuration_api), .numbers = &field, .number_count = 1,
    };
    static const char id[] = MC_PLUGIN_CAPABILITY_CONFIGURATION;
#elif defined(DUPLICATE_CONFIGURATION)
    static const mc_boolean_setting boolean = { .key = { "same", 4 }, .default_value = true };
    static const mc_text_setting text = { .key = { "same", 4 }, .default_value = { "", 0 } };
    static const mc_configuration_api factory = {
        .struct_size = sizeof(mc_configuration_api), .booleans = &boolean, .boolean_count = 1,
        .texts = &text, .text_count = 1,
    };
    static const char id[] = MC_PLUGIN_CAPABILITY_CONFIGURATION;
#elif defined(BAD_INDEXER)
    static const mc_indexer_source_factory factory = MC_INDEXER_SOURCE_FACTORY_DEFAULT;
    static const char id[] = MC_PLUGIN_CAPABILITY_INDEXER_SOURCE;
#elif defined(BAD_ENCODER)
    static const mc_encoder_factory factory = MC_ENCODER_FACTORY_DEFAULT;
    static const char id[] = MC_PLUGIN_CAPABILITY_ENCODER;
#elif defined(BAD_DSP)
    static const mc_dsp_factory factory = MC_DSP_FACTORY_DEFAULT;
    static const char id[] = MC_PLUGIN_CAPABILITY_DSP;
#elif defined(BAD_STREAM)
    static const mc_stream_factory factory = MC_STREAM_FACTORY_DEFAULT;
    static const char id[] = MC_PLUGIN_CAPABILITY_STREAM;
#elif defined(BAD_DECODER)
    static const mc_decoder_factory factory = MC_DECODER_FACTORY_DEFAULT;
    static const char id[] = MC_PLUGIN_CAPABILITY_DECODER;
#else
    static const mc_output_factory factory = MC_OUTPUT_FACTORY_DEFAULT;
    static const char id[] = MC_PLUGIN_CAPABILITY_OUTPUT;
#endif
    mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
    capability.id = (mc_string_view) { id, sizeof(id) - 1 };
    capability.api = &factory;
    capability.api_size = sizeof(factory);
    return host->register_plugin_capability(host->host_context, &capability);
#endif
#if !defined(BAD_OUTPUT) && !defined(BAD_STREAM) && !defined(BAD_DECODER) && !defined(BAD_CONFIGURATION) && \
    !defined(BAD_ENCODER) && !defined(BAD_DSP) && !defined(BAD_INDEXER) && !defined(BAD_CAPABILITY) && \
    !defined(BAD_NUMBER_CONFIGURATION) && !defined(DUPLICATE_CONFIGURATION)
    /* exercise the same mixed setting types needed by index source plugins. */
    static const mc_boolean_setting boolean = { .key = { "enabled", 7 }, .default_value = true };
    static const mc_number_setting number = {
        .key = { "seconds", 7 }, .default_value = 2.5, .minimum = 0.0, .maximum = 10.0, .precision = 1,
    };
    static const mc_text_setting text = { .key = { "label", 5 }, .default_value = { "default", 7 } };
    static const mc_configuration_api settings = {
        .struct_size = sizeof(mc_configuration_api),
        .booleans = &boolean, .boolean_count = 1,
        .numbers = &number, .number_count = 1,
        .texts = &text, .text_count = 1,
    };
    mc_plugin_capability settingsCapability = MC_PLUGIN_CAPABILITY_DEFAULT;
    settingsCapability.id = (mc_string_view) { MC_PLUGIN_CAPABILITY_CONFIGURATION,
        sizeof(MC_PLUGIN_CAPABILITY_CONFIGURATION) - 1 };
    settingsCapability.api = &settings;
    settingsCapability.api_size = sizeof(settings);
    if (host->register_plugin_capability(host->host_context, &settingsCapability) != MC_OK) {
        return MC_ERROR_INTERNAL;
    }
#endif
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
    uint32_t version, const mc_host_api* host, mc_plugin_api* output, mc_error** error) {
    /* reject an unknown host major before reading its table layout. */
    if (MC_ABI_VERSION_MAJOR(version) != MC_ABI_VERSION_MAJOR(MC_PLUGIN_ABI_VERSION) ||
        MC_ABI_VERSION_MINOR(version) < MC_ABI_VERSION_MINOR(MC_PLUGIN_ABI_VERSION))
    {
        return MC_ERROR_UNSUPPORTED;
    }
    if (!host || host->struct_size < sizeof(mc_host_api) || !host->log || !host->create_error || !host->destroy_error ||
        !host->register_plugin_capability || !output || output->struct_size < sizeof(mc_plugin_api) ||
        version != host->abi_version)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    note(host, "negotiate");
    mc_plugin_capability unavailable = MC_PLUGIN_CAPABILITY_DEFAULT;
    if (host->register_plugin_capability(host->host_context, &unavailable) != MC_ERROR_IN_FLIGHT) {
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
