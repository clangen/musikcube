#include <musikcore/plugin.h>
#include <musikcore/plugin/decoder.h>

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* one input byte represents one mono frame at 8 Hz. tiny blocks expose short
reads, final samples, and seeks without relying on a codec or an audio device. */
enum { sample_rate = 8, block_frames = 4 };

typedef struct byte_decoder {
    const mc_audio_scratch_buffer_api* scratch;
    const mc_stream* stream;
    atomic_bool cancelled;
    uint64_t length;
    uint64_t position;
    uint32_t rate;
    int fail_after_prefetch;
    int empty_more;
    int unknown_duration;
} byte_decoder;

/* recognize only the fixture extension so real codecs cannot be masked. */
static mc_result MC_CALL can_handle(void* context, const mc_media_hint* hint, uint32_t* supported) {
    (void) context;
    *supported = hint->extension.length == 5 && memcmp(hint->extension.data, "bytes", 5) == 0;
    return MC_OK;
}

/* each worker owns its own cursor and borrows the host's scratch operations. */
static mc_result MC_CALL create(void* context, const mc_plugin_services* services, void** instance, mc_error** error) {
    (void) context;
    (void) error;
    byte_decoder* decoder = calloc(1, sizeof(*decoder));
    if (!decoder) {
        return MC_ERROR_INTERNAL;
    }
    atomic_init(&decoder->cancelled, 0);
    decoder->scratch = services->audio_scratch;
    decoder->rate = sample_rate;
    *instance = decoder;
    return MC_OK;
}

/* read the size from the real stream so duration and seek share the same units. */
static mc_result MC_CALL open_decoder(void* instance, const mc_stream* stream, mc_error** error) {
    (void) error;
    byte_decoder* decoder = instance;
    if (atomic_load(&decoder->cancelled)) {
        return MC_ERROR_CANCELLED;
    }
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    const mc_result status = stream->api->get_info(stream->instance, &info);
    if (status != MC_OK) {
        return status;
    }
    if (!(info.flags & MC_STREAM_LENGTH_KNOWN)) {
        return MC_ERROR_UNSUPPORTED;
    }
    decoder->stream = stream;
    decoder->length = info.length_bytes;
    /* use filenames to inject provider faults without runtime test hooks. */
    const char fail[] = "read-error.bytes";
    const char empty[] = "empty-more.bytes";
    const char live[] = "live.bytes";
    decoder->fail_after_prefetch = info.uri.length >= sizeof(fail) - 1 &&
        memcmp(info.uri.data + info.uri.length - (sizeof(fail) - 1), fail, sizeof(fail) - 1) == 0;
    decoder->empty_more = info.uri.length >= sizeof(empty) - 1 &&
        memcmp(info.uri.data + info.uri.length - (sizeof(empty) - 1), empty, sizeof(empty) - 1) == 0;
    decoder->unknown_duration = info.uri.length >= sizeof(live) - 1 &&
        memcmp(info.uri.data + info.uri.length - (sizeof(live) - 1), live, sizeof(live) - 1) == 0;
    return MC_OK;
}

/* stop the next decode even if cancellation arrives before open. */
static void MC_CALL interrupt(void* instance) {
    byte_decoder* decoder = instance;
    atomic_store(&decoder->cancelled, 1);
}

/* publish the actual byte count. a read at EOF clears previous scratch frames
so the caller cannot accidentally replay the tail of the track. */
static mc_result MC_CALL decode(
    void* instance, mc_audio_scratch_buffer* buffer, mc_decode_state* state, mc_error** error)
{
    byte_decoder* decoder = instance;
    if (atomic_load(&decoder->cancelled)) {
        return MC_ERROR_CANCELLED;
    }
    if (decoder->fail_after_prefetch && decoder->position >= 40000) {
        return MC_ERROR_IO;
    }
    /* a stale previous block must never conceal a zero-progress decoder. */
    if (decoder->empty_more && decoder->position > 0) {
        *state = MC_DECODE_MORE;
        return MC_OK;
    }
    const mc_stream* stream = decoder->stream;
    unsigned char bytes[block_frames];
    size_t count = 0;
    mc_read_state read_state = MC_READ_MORE;
    mc_result status = stream->api->read(stream->instance, bytes, sizeof(bytes), &count, &read_state, error);
    if (status != MC_OK) {
        return status;
    }
    const mc_audio_scratch_buffer_api* scratch = decoder->scratch;
    const mc_audio_format format = { sizeof(mc_audio_format), decoder->rate, 1, 0 };
    status = scratch->set_format(scratch->context, buffer, &format);
    if (status != MC_OK) {
        return status;
    }
    status = scratch->reserve(scratch->context, buffer, count);
    if (status != MC_OK) {
        return status;
    }
    mc_audio_buffer_view view = MC_AUDIO_BUFFER_VIEW_DEFAULT;
    status = scratch->begin_write(scratch->context, buffer, &view);
    if (status != MC_OK) {
        return status;
    }
    for (size_t i = 0; i < count; ++i) {
        view.data[i] = bytes[i] / 256.0f;
    }
    decoder->position += count;
    *state = read_state == MC_READ_END || decoder->position == decoder->length ? MC_DECODE_END : MC_DECODE_MORE;
    return scratch->end_write(scratch->context, buffer, count);
}

/* seek rounds down to a frame so tests can check the reported actual time. */
static mc_result MC_CALL seek(void* instance, double seconds, double* actual, mc_error** error) {
    byte_decoder* decoder = instance;
    if (!isfinite(seconds) || seconds < 0 || seconds * decoder->rate >= 0x1p64) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    const uint64_t frame = (uint64_t) (seconds * decoder->rate);
    const mc_stream* stream = decoder->stream;
    mc_result status = stream->api->seek(stream->instance, frame, error);
    if (status == MC_OK) {
        decoder->position = frame;
        *actual = (double) frame / decoder->rate;
    }
    return status;
}

/* the fixture has no headers or padding, so file length is its frame count. */
static mc_result MC_CALL duration(void* instance, double* seconds) {
    byte_decoder* decoder = instance;
    if (decoder->unknown_duration) {
        return MC_ERROR_UNSUPPORTED;
    }
    *seconds = (double) decoder->length / decoder->rate;
    return MC_OK;
}

/* use the test device clock when exercising the full playback path. */
static mc_result MC_CALL prefer_rate(void* instance, uint32_t rate) {
    if (rate != sample_rate && rate != 48000) {
        return MC_ERROR_UNSUPPORTED;
    }
    ((byte_decoder*) instance)->rate = rate;
    return MC_OK;
}

/* the host retains the input stream and scratch storage. */
static void MC_CALL destroy(void* instance) { free(instance); }

/* publish one immutable factory for all independent test decoder instances. */
static mc_result MC_CALL initialize(const mc_host_api* host, void** context, mc_error** error) {
    (void) context;
    (void) error;
    static const mc_decoder_vtable methods = {
        .struct_size = sizeof(mc_decoder_vtable),
        .destroy = destroy,
        .open = open_decoder,
        .interrupt = interrupt,
        .decode = decode,
        .seek = seek,
        .get_duration = duration,
        .set_preferred_sample_rate = prefer_rate,
    };
    static const mc_decoder_factory factory = {
        .struct_size = sizeof(mc_decoder_factory),
        .instance_api = &methods,
        .can_handle = can_handle,
        .create = create,
    };
    const mc_plugin_capability capability = {
        .struct_size = sizeof(mc_plugin_capability),
        .id = { MC_PLUGIN_CAPABILITY_DECODER, sizeof(MC_PLUGIN_CAPABILITY_DECODER) - 1 },
        .api = &factory,
        .api_size = sizeof(factory),
    };
    return host->register_capability(host->host_context, &capability);
}

/* the fixture has no session resources once the host destroys its instances. */
static void MC_CALL shutdown_decoder(void* context) { (void) context; }

/* expose only the public C entry point, as an installed third-party codec would. */
MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const mc_result status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = (mc_plugin_api) MC_PLUGIN_API_DEFAULT;
    plugin->id = (mc_string_view) { "test.bytes", 10 };
    plugin->name = (mc_string_view) { "Byte decoder", 12 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown_decoder;
    return MC_OK;
}
