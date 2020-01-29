
#include <stdio.h>
#include <unistd.h>
#include <core/musikcore_c.h>

#define INPUT_FILE "/home/clangen/in.opus"
#define OUTPUT_FILE "/home/clangen/out.flac"

static void test_decode_encode() {
    mcsdk_data_stream in = mcsdk_env_open_data_stream(INPUT_FILE, mcsdk_stream_open_flags_read);
    mcsdk_data_stream out = mcsdk_env_open_data_stream(OUTPUT_FILE, mcsdk_stream_open_flags_write);
    if (mcsdk_handle_ok(in) && mcsdk_handle_ok(out)) {
        printf("test_decode_encode: decoding from %s\n", INPUT_FILE);
        printf("test_decode_encode: encoding to %s\n", OUTPUT_FILE);
        mcsdk_decoder decoder = mcsdk_env_open_decoder(in);
        mcsdk_encoder encoder = mcsdk_env_open_encoder(".ogg");
        if (mcsdk_handle_ok(decoder) && mcsdk_handle_ok(encoder)) {
            printf("test_decode_encode: encoder and decoder opened successfully. running...\n");
            if (mcsdk_encoder_get_type(encoder) == mcsdk_encoder_type_blocking) {
                mcsdk_blocking_encoder be = mcsdk_cast_handle(encoder);
                mcsdk_audio_buffer buffer = mcsdk_env_create_audio_buffer(4096, 44100, 2);
                if (mcsdk_blocking_encoder_initialize(be, out, 44100, 2, 192)) {
                    while (mcsdk_decoder_fill_buffer(decoder, buffer)) {
                        mcsdk_blocking_encoder_encode(be, buffer);
                    }
                    mcsdk_blocking_encoder_finalize(be);
                }
                mcsdk_audio_buffer_release(buffer);
            }
        }
        mcsdk_decoder_release(decoder);
        mcsdk_encoder_release(encoder);
    }
    mcsdk_data_stream_release(in);
    mcsdk_data_stream_release(out);
    printf("test_decode_encode: done.\n");
}

static void test_playback(mcsdk_context* context) {
    printf("test_playback: loading 'a day in the life' tracks\n");
    mcsdk_track_list tl = mcsdk_svc_metadata_query_tracks(
        context->metadata, "a day in the life", mcsdk_no_limit, mcsdk_no_offset);
    mcsdk_svc_playback_play(context->playback, tl, 0);
    mcsdk_track_list_release(tl);
    printf("test_playback: playing for 5 seconds...\n");
    for (int i = 0; i < 5; i++) {
        usleep(1000000);
        printf("  %d\n", i + 1);
    }
    printf("test_playback: done.\n");
}

static void test_metadata(mcsdk_context* context) {
    printf("test_metadata: querying 'beatles' albums\n");
    mcsdk_map_list ml = mcsdk_svc_metadata_query_albums(context->metadata, "beatles");
    int count = mcsdk_map_list_get_count(ml);
    for (int i = 0; i < count; i++) {
        mcsdk_map m = mcsdk_map_list_get_at(ml, i);
        size_t len = mcsdk_map_get_string(m, "album", NULL, 0);
        char* str = malloc(len * sizeof(char));
        mcsdk_map_get_string(m, "album", str, len);
        printf("  %s\n", str);
        free(str);
    }
    mcsdk_map_list_release(ml);
    printf("test_metadata: done.\n");
}

int main(int argc, char** argv) {
    mcsdk_context* context = NULL;
    mcsdk_context_init(&context);
    if (context) {
        printf("main: context initialized\n");
        test_metadata(context);
        test_playback(context);
        test_decode_encode(context);
        mcsdk_context_release(&context);
        printf("main: context released\n");
    }
    return 0;
}
