#include <musikcore/plugin.h>
#include <musikcore/plugin/audio.h>
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/indexer.h>
#include <musikcore/plugin/tag.h>

#include <stdio.h>
#include <stdlib.h>

/* defaults are safe empty descriptions, not already callable objects. check
their byte prefixes because hosts use them to validate cross-library layouts. */
static void require(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "plugin header defaults: %s\n", message);
        abort();
    }
}

/* bootstrap callbacks are present but never invoked by entry validation. */
static unsigned bootstrap_calls = 0;

/* count accidental logging during a validation-only helper. */
static void MC_CALL log_message(void* context, mc_log_level level, mc_string_view tag, mc_string_view text) {
    (void) context;
    (void) level;
    (void) tag;
    (void) text;
    ++bootstrap_calls;
}

/* entry validation must not allocate error objects through host callbacks. */
static mc_result MC_CALL create_error(void* context, mc_result code, mc_string_view text, mc_error** error) {
    (void) context;
    (void) code;
    (void) text;
    (void) error;
    ++bootstrap_calls;
    return MC_ERROR_INTERNAL;
}

/* entry validation must not destroy resources through host callbacks. */
static void MC_CALL destroy_error(void* context, mc_error** error) {
    (void) context;
    (void) error;
    ++bootstrap_calls;
}

/* registration belongs to initialize, never entry validation. */
static mc_result MC_CALL register_plugin_capability(void* context, const mc_plugin_capability* capability) {
    (void) context;
    (void) capability;
    ++bootstrap_calls;
    return MC_OK;
}

/* one shared helper owns version/prefix checks. reject malformed inputs without
mutating the plugin table, accept future host minors, and preserve output tails. */
static void checkEntryValidation(void) {
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = log_message;
    host.create_error = create_error;
    host.destroy_error = destroy_error;
    host.register_plugin_capability = register_plugin_capability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_OK,
        "valid host passes entry validation");
    require(plugin.struct_size == sizeof(plugin) && plugin.abi_version == MC_PLUGIN_ABI_VERSION && !plugin.initialize,
        "entry validation preserves the caller table");

    /* compatibility uses major/minor independently of application ABI versions. */
    require(mc_plugin_validate_host_api(MC_ABI_VERSION_ENCODE(1, 0), &host, &plugin, NULL) == MC_ERROR_UNSUPPORTED,
        "different plugin ABI major is rejected");
    require(
        mc_plugin_validate_host_api(0, &host, &plugin, NULL) == MC_ERROR_UNSUPPORTED, "missing plugin ABI is rejected");
    host.abi_version = MC_ABI_VERSION_ENCODE(0, 0);
    require(mc_plugin_validate_host_api(host.abi_version, &host, &plugin, NULL) == MC_ERROR_UNSUPPORTED,
        "older host minor is rejected");
    host.abi_version = MC_ABI_VERSION_ENCODE(0, 2);
    require(mc_plugin_validate_host_api(host.abi_version, &host, &plugin, NULL) == MC_OK,
        "newer compatible host minor is accepted");
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "host argument and table must agree on the negotiated version");
    host.abi_version = MC_PLUGIN_ABI_VERSION;

    /* invalid prefixes and missing callbacks fail before touching output fields. */
    plugin.name.length = 123;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, NULL, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT &&
            plugin.name.length == 123,
        "null host leaves output unchanged");
    host.struct_size = sizeof(uint32_t);
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "truncated host is rejected");
    host.struct_size = sizeof(host);
    host.log = NULL;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "missing log callback is rejected");
    host.log = log_message;
    host.create_error = NULL;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "missing error allocator is rejected");
    host.create_error = create_error;
    host.destroy_error = NULL;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "missing error destructor is rejected");
    host.destroy_error = destroy_error;
    host.register_plugin_capability = NULL;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "missing registrar is rejected");
    host.register_plugin_capability = register_plugin_capability;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT,
        "null output table is rejected");
    plugin.struct_size = sizeof(uint32_t);
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &plugin, NULL) == MC_ERROR_INVALID_ARGUMENT &&
            plugin.name.length == 123,
        "truncated output table is unchanged");

    /* a newer caller's tail is not part of this SDK's writable prefix. */
    struct ExtendedPlugin {
        mc_plugin_api api;
        uint64_t tail;
    } extended = { MC_PLUGIN_API_DEFAULT, 1234 };
    extended.api.struct_size = sizeof(extended);
    mc_error* error = NULL;
    require(mc_plugin_validate_host_api(MC_PLUGIN_ABI_VERSION, &host, &extended.api, &error) == MC_OK && !error &&
            extended.tail == 1234 && extended.api.struct_size == sizeof(extended),
        "entry validation preserves the caller prefix and future storage");
    require(bootstrap_calls == 0, "entry validation calls no host services");
}

/* compile and inspect each new initializer from a strict C11 translation unit. */
int main(void) {
    checkEntryValidation();
    const mc_cancel_token cancel = MC_CANCEL_TOKEN_DEFAULT;
    const mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
    const mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
    const mc_audio_scratch_buffer_api audio = MC_AUDIO_SCRATCH_BUFFER_API_DEFAULT;
    const mc_output output = MC_OUTPUT_DEFAULT;
    const mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    const mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    const mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
    const mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
    const mc_output_vtable methods = MC_OUTPUT_VTABLE_DEFAULT;
    const mc_output_factory factory = MC_OUTPUT_FACTORY_DEFAULT;
    const mc_indexer_writer writer = MC_INDEXER_WRITER_DEFAULT;
    const mc_indexer_source_info source = MC_INDEXER_SOURCE_INFO_DEFAULT;
    const mc_indexer_source_host_api source_host = MC_INDEXER_SOURCE_HOST_API_DEFAULT;
    const mc_indexer_source_vtable source_methods = MC_INDEXER_SOURCE_VTABLE_DEFAULT;
    const mc_indexer_source_factory source_factory = MC_INDEXER_SOURCE_FACTORY_DEFAULT;
    const mc_tag_reader_factory tag_factory = MC_TAG_READER_FACTORY_DEFAULT;

    /* unset cancellation and PCM expose no implicit resource lifetime. */
    require(cancel.struct_size == sizeof(cancel) && !cancel.context && !cancel.is_cancelled, "cancel");
    require(pcm.struct_size == sizeof(pcm) && !pcm.data && !pcm.frame_count && !pcm.format.sample_rate, "PCM");
    require(hint.struct_size == sizeof(hint) && !hint.uri.length && !hint.extension.length, "media hint");
    require(
        audio.struct_size == sizeof(audio) && !audio.create && !audio.begin_write && !audio.end_write, "audio host");

    /* defaults do not accidentally claim a live instance, token, or device clock. */
    require(output.struct_size == sizeof(output) && !output.instance && !output.api, "output");
    require(callbacks.struct_size == sizeof(callbacks) && !callbacks.completed && !callbacks.ready, "callbacks");
    require(options.struct_size == sizeof(options) && options.initial_volume == 1 && !options.queue_capacity_frames,
        "output options");
    require(buffer.struct_size == sizeof(buffer) && !buffer.token && !buffer.pcm.data, "buffer");
    require(timing.struct_size == sizeof(timing) && !timing.valid_flags && !timing.generation, "timing");
    require(methods.struct_size == sizeof(methods) && !methods.destroy && !methods.queue_audio_buffer && !methods.stop,
        "vtable");
    require(factory.struct_size == sizeof(factory) && !factory.instance_api && !factory.create, "factory");
    /* an empty source cannot accidentally write, rescan, or claim an identity. */
    require(writer.struct_size == sizeof(writer) && !writer.create_record && !writer.save, "indexer writer");
    require(source.struct_size == sizeof(source) && !source.source_key.length && !source.stable_ids, "indexer source");
    require(source_host.struct_size == sizeof(source_host) && !source_host.request_rescan, "indexer source host");
    require(source_methods.struct_size == sizeof(source_methods) && !source_methods.scan && !source_methods.interrupt,
        "indexer source operations");
    require(source_factory.struct_size == sizeof(source_factory) && !source_factory.create, "indexer source factory");
    require(tag_factory.struct_size == sizeof(tag_factory) && !tag_factory.create, "tag reader factory");
    return 0;
}
