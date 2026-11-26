#include <musikcore/media.h>
#include "media-test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "media contract failed at line %d: %s\n", __LINE__, #condition);                           \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)

int media_cpp_contract(const char* directory, const char* input, const char* output);

static mc_string_view text(const char* value) {
    return (mc_string_view) {
        .data = value,
        .length = strlen(value),
    };
}

static mc_context* context_create(const char* directory) {
    mc_context* context = NULL;
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_PLUGINS;
    options.plugin_directory = text(directory);
    CHECK(mc_context_create(&options, &context, NULL) == MC_OK);
    return context;
}

static mc_media_stream* stream_create(mc_context* context, const char* uri, uint32_t flags) {
    mc_media_stream* stream = NULL;
    CHECK(mc_media_stream_create(context, text(uri), flags, &stream, NULL) == MC_OK);
    return stream;
}

static mc_media_stream* stream_open(mc_context* context, const char* uri, uint32_t flags) {
    mc_media_stream* stream = stream_create(context, uri, flags);
    CHECK(mc_media_stream_open(stream, NULL) == MC_OK);
    return stream;
}

static mc_media_encoder_options encoder_options(void) {
    mc_media_encoder_options options = MC_MEDIA_ENCODER_OPTIONS_DEFAULT;
    options.format.sample_rate = 8;
    options.format.channels = 1;
    options.bitrate_kbps = 32;
    return options;
}

static int create_on_worker(void* data) {
    mc_media_stream* stream = NULL;
    const mc_result status = mc_media_stream_create(data, text("testmedia:wrong-thread"),
        MC_MEDIA_OPEN_READ, &stream, NULL);
    CHECK(!stream);
    return status;
}

static void invalid_inputs(mc_context* context) {
    mc_media_stream* stream = NULL;
    mc_media_decoder* decoder = NULL;
    mc_media_encoder* encoder = NULL;
    mc_media_encoder_options options = encoder_options();
    int calls = media_fixture_control(0);
    CHECK(media_worker_start(create_on_worker, context));
    CHECK(media_worker_join() == MC_ERROR_WRONG_THREAD);
    CHECK(calls == media_fixture_control(0));
    CHECK(mc_media_stream_create(context, text("testmedia:unused"), 0, &stream, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_create(context, text("testmedia:unused"), 0x80000001u, &stream, NULL) ==
        MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_create(context, text("testmedia:unused"), MC_MEDIA_OPEN_READ | MC_MEDIA_OPEN_CREATE, &stream,
              NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_create(context, text("testmedia:unused"),
              MC_MEDIA_OPEN_WRITE | MC_MEDIA_OPEN_APPEND | MC_MEDIA_OPEN_TRUNCATE, &stream,
              NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_create(context, (mc_string_view) {
        .data = "testmedia:x\0y",
        .length = 13,
    }, MC_MEDIA_OPEN_READ, &stream,
              NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_create(context, (mc_string_view) {
        .data = NULL,
        .length = 12,
    }, MC_MEDIA_OPEN_READ, &stream, NULL) ==
        MC_ERROR_INVALID_ARGUMENT);
    CHECK(!stream && calls == media_fixture_control(0));
    stream = stream_create(context, "testmedia:unused", MC_MEDIA_OPEN_READ);
    calls = media_fixture_control(0);
    CHECK(mc_media_decoder_create(&stream, &decoder, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(stream && !decoder && calls == media_fixture_control(0));
    CHECK(mc_media_stream_open(stream, NULL) == MC_OK);
    calls = media_fixture_control(0);
    CHECK(mc_media_stream_create(context, text("testmedia:unused"), MC_MEDIA_OPEN_READ, &stream, NULL) ==
        MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_encoder_create(&stream, text(".bytes"), &options, &encoder, NULL) == MC_ERROR_INVALID_ARGUMENT);
    options.format.channels = 0;
    CHECK(mc_media_encoder_create(&stream, text(".bytes"), &options, &encoder, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(stream && !encoder && calls == media_fixture_control(0));
    size_t count = 123;
    mc_media_read_state state = MC_MEDIA_MORE;
    CHECK(mc_media_stream_read(stream, NULL, 1, &count, &state, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_write(stream, "x", 1, &count, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(calls == media_fixture_control(0));
    CHECK(mc_media_stream_destroy(&stream) == MC_OK);
}

static void local_io(mc_context* context, const char* path) {
    const unsigned char bytes[] = { 64, 128, 192, 255, 32 };
    mc_media_stream* stream =
        stream_open(context, path, MC_MEDIA_OPEN_WRITE | MC_MEDIA_OPEN_CREATE | MC_MEDIA_OPEN_TRUNCATE);
    size_t count = 0;
    CHECK(mc_media_stream_write(stream, bytes, sizeof(bytes), &count, NULL) == MC_OK && count == sizeof(bytes));
    mc_media_stream_info current = MC_MEDIA_STREAM_INFO_DEFAULT;
    CHECK(mc_media_stream_get_info(stream, &current) == MC_OK && current.length_bytes == sizeof(bytes));
    uint64_t offset = 0;
    CHECK(mc_media_stream_tell(stream, &offset) == MC_OK && offset == sizeof(bytes));
    CHECK(mc_media_stream_destroy(&stream) == MC_OK && !stream);
    stream = stream_open(context, path, MC_MEDIA_OPEN_READ);
    mc_media_stream_info info = MC_MEDIA_STREAM_INFO_DEFAULT;
    CHECK(mc_media_stream_get_info(stream, &info) == MC_OK && info.length_bytes == sizeof(bytes));
    unsigned char read[8];
    mc_media_read_state state = MC_MEDIA_MORE;
    CHECK(mc_media_stream_read(stream, read, 2, &count, &state, NULL) == MC_OK && count == 2);
    CHECK(mc_media_stream_seek(stream, 0, NULL) == MC_OK);
    CHECK(mc_media_stream_read(stream, read, sizeof(read), &count, &state, NULL) == MC_OK && count == sizeof(bytes));
    CHECK(memcmp(read, bytes, sizeof(bytes)) == 0);
    CHECK(mc_media_stream_read(stream, read, sizeof(read), &count, &state, NULL) == MC_OK && !count &&
        state == MC_MEDIA_END);
    CHECK(mc_media_stream_destroy(&stream) == MC_OK);
}

static mc_media_pcm* decode(mc_context* context, const char* path, mc_media_decoder** retained) {
    mc_media_stream* stream = stream_open(context, path, MC_MEDIA_OPEN_READ);
    mc_media_decoder* decoder = NULL;
    CHECK(mc_media_decoder_create(&stream, &decoder, NULL) == MC_OK && !stream);
    CHECK(mc_media_decoder_open(decoder, NULL) == MC_OK);
    double seconds = 0;
    CHECK(mc_media_decoder_get_duration(decoder, &seconds) == MC_OK && seconds == 0.625);
    mc_media_pcm* pcm = NULL;
    mc_media_read_state state = MC_MEDIA_MORE;
    CHECK(mc_media_decoder_read(decoder, &pcm, &state, NULL) == MC_OK && pcm && state == MC_MEDIA_MORE);
    mc_media_pcm_view view = MC_MEDIA_PCM_VIEW_DEFAULT;
    CHECK(mc_media_pcm_get_view(pcm, &view) == MC_OK && view.frame_count == 4 && view.format.channels == 1);
    CHECK(view.data[0] == 0.25f && view.data[1] == 0.5f);
    int calls = media_fixture_control(0);
    CHECK(mc_media_decoder_seek(decoder, NAN, &seconds, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(calls == media_fixture_control(0));
    mc_media_pcm* tail = NULL;
    CHECK(mc_media_decoder_read(decoder, &tail, &state, NULL) == MC_OK && tail && state == MC_MEDIA_END);
    mc_media_pcm_destroy(&tail);
    CHECK(mc_media_decoder_read(decoder, &tail, &state, NULL) == MC_OK && !tail && state == MC_MEDIA_END);
    CHECK(view.data[0] == 0.25f);
    CHECK(mc_media_decoder_seek(decoder, 0.2, &seconds, NULL) == MC_OK && seconds == 0.125);
    CHECK(mc_media_decoder_read(decoder, &tail, &state, NULL) == MC_OK && tail);
    mc_media_pcm_destroy(&tail);
    *retained = decoder;
    return pcm;
}

static void encode(mc_context* context, const char* path, const char* format) {
    mc_media_stream* stream =
        stream_open(context, path, MC_MEDIA_OPEN_WRITE | MC_MEDIA_OPEN_CREATE | MC_MEDIA_OPEN_TRUNCATE);
    mc_media_encoder* encoder = NULL;
    mc_media_encoder_options options = encoder_options();
    CHECK(mc_media_encoder_create(&stream, text("unsupported-fixture-format"), &options, &encoder, NULL) ==
        MC_ERROR_UNSUPPORTED);
    CHECK(stream && !encoder);
    CHECK(mc_media_encoder_create(&stream, text(format), &options, &encoder, NULL) == MC_OK && !stream);
    CHECK(mc_media_encoder_open(encoder, NULL) == MC_OK);
    const float samples[] = { 0.1f, 0.2f, 0.3f };
    mc_media_pcm_view view = MC_MEDIA_PCM_VIEW_DEFAULT;
    view.data = samples;
    view.frame_count = 3;
    view.format = options.format;
    view.format.sample_rate = 16;
    CHECK(mc_media_encoder_write(encoder, &view, NULL) == MC_ERROR_INVALID_ARGUMENT);
    view.format.sample_rate = 8;
    CHECK(mc_media_encoder_write(encoder, &view, NULL) == MC_OK);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_destroy(&encoder) == MC_OK);
    stream = stream_open(context, path, MC_MEDIA_OPEN_READ);
    unsigned char bytes[8];
    size_t count = 0;
    mc_media_read_state state = MC_MEDIA_MORE;
    CHECK(mc_media_stream_read(stream, bytes, sizeof(bytes), &count, &state, NULL) == MC_OK && count == 4);
    CHECK(bytes[0] == 1 && bytes[1] == 2 && bytes[2] == 3 && bytes[3] == '!');
    CHECK(mc_media_stream_destroy(&stream) == MC_OK);
}

static void errors(mc_context* context) {
    media_fixture_control(5);
    mc_media_stream* failed = NULL;
    mc_error* error = NULL;
    CHECK(mc_media_stream_create(context, text("testmedia:factory-fail"), MC_MEDIA_OPEN_READ, &failed, &error) ==
            MC_ERROR_IO &&
        !failed);
    mc_error_destroy(&error);
    mc_media_stream* stream = stream_create(context, "testmedia:open-fail", MC_MEDIA_OPEN_READ);
    CHECK(mc_media_stream_open(stream, &error) == MC_ERROR_IO && error);
    CHECK(mc_error_get_code(error) == MC_ERROR_IO && mc_error_get_message(error).length);
    mc_error_destroy(&error);
    CHECK(mc_media_stream_open(stream, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_media_stream_destroy(&stream) == MC_OK);
    stream = stream_open(context, "testmedia:unknown-length", MC_MEDIA_OPEN_READ);
    mc_media_decoder* decoder = NULL;
    CHECK(mc_media_decoder_create(&stream, &decoder, NULL) == MC_OK && !stream);
    CHECK(mc_media_decoder_open(decoder, NULL) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_media_decoder_destroy(&decoder) == MC_OK && !decoder);
    stream = stream_create(context, "testmedia:block-open", MC_MEDIA_OPEN_READ);
    CHECK(mc_media_stream_interrupt(stream) == MC_OK);
    CHECK(mc_media_stream_open(stream, NULL) == MC_ERROR_CANCELLED);
    CHECK(mc_media_stream_destroy(&stream) == MC_OK);
    stream = stream_open(context, "testmedia:close-fail", MC_MEDIA_OPEN_READ);
    CHECK(mc_media_stream_destroy(&stream) == MC_ERROR_IO && !stream);
    stream = stream_open(context, "testmedia:write-fail", MC_MEDIA_OPEN_WRITE);
    size_t count = 0;
    CHECK(mc_media_stream_write(stream, "xyz", 3, &count, NULL) == MC_ERROR_IO && count == 1);
    mc_media_encoder_options options = encoder_options();
    mc_media_encoder* encoder = NULL;
    CHECK(mc_media_encoder_create(&stream, text(".bytes"), &options, &encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_open(encoder, NULL) == MC_OK);
    float sample = 0.5f;
    mc_media_pcm_view view = MC_MEDIA_PCM_VIEW_DEFAULT;
    view.data = &sample;
    view.frame_count = 1;
    view.format = options.format;
    CHECK(mc_media_encoder_write(encoder, &view, NULL) == MC_ERROR_IO);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_ERROR_IO);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_ERROR_IO);
    CHECK(mc_media_encoder_destroy(&encoder) == MC_OK);
    stream = stream_open(context, "testmedia:cancel-encoder", MC_MEDIA_OPEN_WRITE);
    CHECK(mc_media_encoder_create(&stream, text(".bytes"), &options, &encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_interrupt(encoder) == MC_OK);
    CHECK(mc_media_encoder_open(encoder, NULL) == MC_ERROR_CANCELLED);
    CHECK(mc_media_encoder_destroy(&encoder) == MC_OK);
    stream = stream_open(context, "testmedia:finalize-fail", MC_MEDIA_OPEN_WRITE);
    CHECK(mc_media_encoder_create(&stream, text(".failbytes"), &options, &encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_open(encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_ERROR_IO);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_ERROR_IO);
    CHECK(mc_media_encoder_destroy(&encoder) == MC_OK);
    stream = stream_open(context, "testmedia:partial-write", MC_MEDIA_OPEN_WRITE);
    CHECK(mc_media_encoder_create(&stream, text(".bytes"), &options, &encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_open(encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_write(encoder, &view, NULL) == MC_OK);
    CHECK(mc_media_encoder_finish(encoder, NULL) == MC_OK);
    CHECK(mc_media_encoder_destroy(&encoder) == MC_OK);
}

typedef struct blocking_call {
    int mode;
    mc_media_stream* stream;
    mc_media_decoder* decoder;
    mc_media_encoder* encoder;
} blocking_call;

static int block(void* data) {
    blocking_call* call = data;
    if (call->mode == 0) {
        return mc_media_stream_open(call->stream, NULL);
    }
    if (call->mode == 1) {
        unsigned char byte;
        size_t count = 0;
        mc_media_read_state state = MC_MEDIA_MORE;
        return mc_media_stream_read(call->stream, &byte, 1, &count, &state, NULL);
    }
    if (call->mode == 2) {
        mc_media_pcm* pcm = NULL;
        mc_media_read_state state = MC_MEDIA_MORE;
        return mc_media_decoder_read(call->decoder, &pcm, &state, NULL);
    }
    float sample = 0.5f;
    mc_media_pcm_view view = MC_MEDIA_PCM_VIEW_DEFAULT;
    view.data = &sample;
    view.frame_count = 1;
    view.format.sample_rate = 8;
    view.format.channels = 1;
    return mc_media_encoder_write(call->encoder, &view, NULL);
}

static void cancellation(const char* directory, int mode) {
    mc_context* context = context_create(directory);
    const char* uris[] = { "testmedia:block-open", "testmedia:block-read", "testmedia:block-decode",
        "testmedia:block-write" };
    blocking_call call = {
        .mode = mode,
        .stream = NULL,
        .decoder = NULL,
        .encoder = NULL,
    };
    call.stream = stream_create(context, uris[mode], mode == 3 ? MC_MEDIA_OPEN_WRITE : MC_MEDIA_OPEN_READ);
    if (mode) {
        CHECK(mc_media_stream_open(call.stream, NULL) == MC_OK);
    }
    if (mode == 2) {
        CHECK(mc_media_decoder_create(&call.stream, &call.decoder, NULL) == MC_OK);
        CHECK(mc_media_decoder_open(call.decoder, NULL) == MC_OK);
    }
    if (mode == 3) {
        mc_media_encoder_options options = encoder_options();
        CHECK(mc_media_encoder_create(&call.stream, text(".bytes"), &options, &call.encoder, NULL) == MC_OK);
        CHECK(mc_media_encoder_open(call.encoder, NULL) == MC_OK);
    }
    media_fixture_control(3);
    CHECK(media_worker_start(block, &call));
    CHECK(media_fixture_control(2));
    if (call.stream) {
        CHECK(mc_media_stream_destroy(&call.stream) == MC_ERROR_BUSY && call.stream);
    }
    if (call.decoder) {
        CHECK(mc_media_decoder_destroy(&call.decoder) == MC_ERROR_BUSY && call.decoder);
    }
    if (call.encoder) {
        CHECK(mc_media_encoder_destroy(&call.encoder) == MC_ERROR_BUSY && call.encoder);
    }
    if (mode == 1) {
        CHECK(mc_media_stream_interrupt(call.stream) == MC_OK);
        CHECK(media_worker_join() == MC_ERROR_CANCELLED);
    }
    CHECK(mc_context_shutdown(context) == MC_OK);
    if (mode != 1) {
        CHECK(media_worker_join() == MC_ERROR_CANCELLED);
    }
    CHECK(media_fixture_control(1) == 0);
    CHECK(mc_context_destroy(&context) == MC_OK);
    if (call.stream) {
        CHECK(mc_media_stream_interrupt(call.stream) == MC_ERROR_NOT_FOUND);
    }
    CHECK(mc_media_stream_destroy(&call.stream) == MC_OK);
    CHECK(mc_media_decoder_destroy(&call.decoder) == MC_OK);
    CHECK(mc_media_encoder_destroy(&call.encoder) == MC_OK);
}

int main(int argc, char** argv) {
    CHECK(argc == 5 || argc == 6);
    CHECK(media_fixture_load(argv[2]));
    mc_context* context = context_create(argv[1]);
    invalid_inputs(context);
    local_io(context, argv[3]);
    mc_media_decoder* decoder = NULL;
    mc_media_pcm* pcm = decode(context, argv[3], &decoder);
    encode(context, argv[4], ".bytes");
    encode(context, argv[4], ".filebytes");
    errors(context);
    if (argc == 6) {
        mc_media_stream* remote = stream_open(context, argv[5], MC_MEDIA_OPEN_READ);
        char bytes[8];
        size_t count = 0;
        mc_media_read_state state = MC_MEDIA_MORE;
        CHECK(mc_media_stream_read(remote, bytes, sizeof(bytes), &count, &state, NULL) == MC_OK && count > 0);
        CHECK(mc_media_stream_destroy(&remote) == MC_OK);
    }
    mc_media_stream* retained = stream_open(context, "testmedia:retained", MC_MEDIA_OPEN_READ);
    CHECK(mc_context_shutdown(context) == MC_OK);
    CHECK(media_fixture_control(1) == 0);
    CHECK(mc_context_destroy(&context) == MC_OK);
    mc_media_stream_info info = MC_MEDIA_STREAM_INFO_DEFAULT;
    CHECK(mc_media_stream_get_info(retained, &info) == MC_ERROR_NOT_FOUND);
    double seconds = 0;
    CHECK(mc_media_decoder_get_duration(decoder, &seconds) == MC_ERROR_NOT_FOUND);
    mc_media_pcm_view view = MC_MEDIA_PCM_VIEW_DEFAULT;
    CHECK(mc_media_pcm_get_view(pcm, &view) == MC_OK && view.data[0] == 0.25f);
    CHECK(mc_media_stream_destroy(&retained) == MC_OK);
    CHECK(mc_media_decoder_destroy(&decoder) == MC_OK);
    mc_media_pcm_destroy(&pcm);
    mc_media_pcm_destroy(&pcm);
    for (int mode = 0; mode < 4; ++mode) {
        cancellation(argv[1], mode);
    }
    CHECK(media_fixture_control(4) == 5);
    CHECK(media_cpp_contract(argv[1], argv[3], argv[4]));
    return 0;
}
