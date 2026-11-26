#include <musikcore/plugin.h>
#include <musikcore/plugin/encoder.h>

#include <stdlib.h>
#include <string.h>

/* each input frame produces one byte. instances have separate output storage
so the host can verify that one response does not overwrite another. */
typedef struct byte_encoder {
    unsigned char bytes[8];
    size_t calls;
    int initialized;
} byte_encoder;

typedef struct fixture {
    mc_encoder_factory factory;
    unsigned instances;
} fixture;

typedef struct owned_encoder {
    byte_encoder encoder;
    fixture* owner;
    const mc_stream* output;
    int file_mode;
    int fail_finalize;
} owned_encoder;

static void MC_CALL destroy(void* opaque) {
    owned_encoder* encoder = opaque;
    if (encoder->output) {
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        const mc_result status = encoder->output->api->get_info(encoder->output->instance, &info);
        if (status != MC_OK && status != MC_ERROR_CANCELLED) {
            abort();
        }
    }
    --encoder->owner->instances;
    free(encoder);
}

static mc_result MC_CALL initialize(
    void* opaque, const mc_stream* output, uint32_t rate, uint32_t channels, uint32_t bitrate)
{
    owned_encoder* encoder = opaque;
    if ((!encoder->file_mode && output) || (encoder->file_mode && !output) || rate != 8 || channels != 1 ||
        bitrate != 32)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    encoder->output = output;
    encoder->encoder.initialized = 1;
    return MC_OK;
}

static mc_result emit(owned_encoder* encoder, size_t length, mc_encoded_block* block) {
    if (!encoder->file_mode) {
        block->data = (const char*) encoder->encoder.bytes;
        block->length = length;
        return MC_OK;
    }
    size_t offset = 0;
    while (offset < length) {
        size_t count = 0;
        const mc_result status = encoder->output->api->write(
            encoder->output->instance, encoder->encoder.bytes + offset, length - offset, &count, NULL);
        if (status != MC_OK) {
            return status;
        }
        if (!count || count > length - offset) {
            return MC_ERROR_PLUGIN;
        }
        offset += count;
    }
    return MC_OK;
}

static mc_result MC_CALL encode(void* opaque, const mc_pcm_view* pcm, mc_encoded_block* output) {
    owned_encoder* encoder = opaque;
    if (!encoder->encoder.initialized || pcm->frame_count > sizeof(encoder->encoder.bytes)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < pcm->frame_count; ++i) {
        encoder->encoder.bytes[i] = (unsigned char) (pcm->data[i] * 10.0f + encoder->encoder.calls);
    }
    ++encoder->encoder.calls;
    return emit(encoder, pcm->frame_count, output);
}

static mc_result MC_CALL flush(void* opaque, mc_encoded_block* output) {
    owned_encoder* encoder = opaque;
    encoder->encoder.bytes[0] = '!';
    return emit(encoder, 1, output);
}

static mc_result MC_CALL finalize(void* opaque, mc_string_view uri) {
    (void) uri;
    return ((owned_encoder*) opaque)->fail_finalize ? MC_ERROR_IO : MC_OK;
}

static const mc_encoder_vtable methods = {
    .struct_size = sizeof(mc_encoder_vtable),
    .destroy = destroy,
    .streaming = 1,
    .initialize = initialize,
    .encode = encode,
    .flush = flush,
    .finalize = finalize,
};

static const mc_encoder_vtable file_methods = {
    .struct_size = sizeof(mc_encoder_vtable),
    .destroy = destroy,
    .streaming = 0,
    .initialize = initialize,
    .encode = encode,
    .flush = flush,
    .finalize = finalize,
};

static mc_result MC_CALL can_handle(void* context, mc_string_view format, uint32_t* supported) {
    (void) context;
    *supported = (format.length == 6 && memcmp(format.data, ".bytes", 6) == 0) ||
        (format.length == 10 && memcmp(format.data, ".filebytes", 10) == 0) ||
        (format.length == 10 && memcmp(format.data, ".failbytes", 10) == 0);
    return MC_OK;
}

static mc_result MC_CALL create(
    void* context, const mc_plugin_services* services, mc_string_view format, mc_encoder* output, mc_error** error)
{
    (void) services;
    (void) format;
    (void) error;
    owned_encoder* encoder = calloc(1, sizeof(*encoder));
    if (!encoder) {
        return MC_ERROR_INTERNAL;
    }
    encoder->owner = context;
    ++encoder->owner->instances;
    output->instance = encoder;
    encoder->file_mode = format.length == 10 && memcmp(format.data, ".filebytes", 10) == 0;
    encoder->fail_finalize = format.length == 10 && memcmp(format.data, ".failbytes", 10) == 0;
    output->api = encoder->file_mode ? &file_methods : &methods;
    return MC_OK;
}

static mc_result MC_CALL start(const mc_host_api* host, void** context, mc_error** error) {
    (void) error;
    fixture* state = calloc(1, sizeof(*state));
    if (!state) {
        return MC_ERROR_INTERNAL;
    }
    *context = state;
    state->factory = (mc_encoder_factory) { sizeof(mc_encoder_factory), state, can_handle, create };
    mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
    capability.id = (mc_string_view) { MC_PLUGIN_CAPABILITY_ENCODER, sizeof(MC_PLUGIN_CAPABILITY_ENCODER) - 1 };
    capability.api = &state->factory;
    capability.api_size = sizeof(state->factory);
    return host->register_plugin_capability(host->host_context, &capability);
}

/* a live encoder at unload makes this test fail instead of hiding a use-after-free. */
static void MC_CALL stop(void* context) {
    fixture* state = context;
    if (state && state->instances) {
        abort();
    }
    free(state);
}

MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const mc_result status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = (mc_plugin_api) MC_PLUGIN_API_DEFAULT;
    plugin->id = (mc_string_view) { "test.bytes.encoder", 18 };
    plugin->name = (mc_string_view) { "byte encoder", 12 };
    plugin->version = (mc_string_view) { "1", 1 };
    plugin->author = (mc_string_view) { "tests", 5 };
    plugin->initialize = start;
    plugin->shutdown = stop;
    return MC_OK;
}
