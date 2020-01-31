#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <core/musikcore_c.h>

#define ENCODER_TYPE ".opus"

#ifdef WIN32
    #define INPUT_FILE "c:\\clangen\\in.mp3"
    #define OUTPUT_FILE "c:\\clangen\\out.opus"
#elif defined __APPLE__
    #define INPUT_FILE "/Users/clangen/in.mp3"
    #define OUTPUT_FILE "/Users/clangen/out.opus"
#else
    #define INPUT_FILE "/home/clangen/in.mp3"
    #define OUTPUT_FILE "/home/clangen/out.mp3"
#endif

static void internal_sleep(int seconds) {
#ifdef WIN32
    Sleep(seconds * 1000);
#else
    usleep(seconds * 1000000);
#endif
}

static void test_decode_encode() {
    mcsdk_data_stream in = mcsdk_env_open_data_stream(INPUT_FILE, mcsdk_stream_open_flags_read);
    mcsdk_data_stream out = mcsdk_env_open_data_stream(OUTPUT_FILE, mcsdk_stream_open_flags_write);
    if (mcsdk_handle_ok(in) && mcsdk_handle_ok(out)) {
        printf("test_decode_encode: decoding from %s\n", INPUT_FILE);
        printf("test_decode_encode: encoding to %s\n", OUTPUT_FILE);
        mcsdk_decoder decoder = mcsdk_env_open_decoder(in);
        mcsdk_encoder encoder = mcsdk_env_open_encoder(ENCODER_TYPE);
        if (mcsdk_handle_ok(decoder) && mcsdk_handle_ok(encoder)) {
            printf("test_decode_encode: encoder and decoder opened successfully. running...\n");
            mcsdk_audio_buffer buffer = mcsdk_env_create_audio_buffer(4096, 44100, 2);
            if (mcsdk_encoder_get_type(encoder) == mcsdk_encoder_type_blocking) {
                mcsdk_blocking_encoder be = mcsdk_cast_handle(encoder);
                if (mcsdk_blocking_encoder_initialize(be, out, 44100, 2, 192)) {
                    while (mcsdk_decoder_fill_buffer(decoder, buffer)) {
                        mcsdk_blocking_encoder_encode(be, buffer);
                    }
                    mcsdk_blocking_encoder_finalize(be);
                }
            } else if (mcsdk_encoder_get_type(encoder) == mcsdk_encoder_type_streaming) {
                mcsdk_streaming_encoder se = mcsdk_cast_handle(encoder);
                if (mcsdk_streaming_encoder_initialize(se, 44100, 2, 192)) {
                    char* bytes = NULL;
                    int length = 0;
                    while (mcsdk_decoder_fill_buffer(decoder, buffer)) {
                        length = mcsdk_streaming_encoder_encode(se, buffer, &bytes);
                        if (bytes && length) {
                            mcsdk_data_stream_write(out, bytes, length);
                        }
                    }
                    length = mcsdk_streaming_encoder_flush(se, &bytes);
                    if (bytes && length) {
                        mcsdk_data_stream_write(out, bytes, length);
                    }
                    mcsdk_data_stream_close(out);
                    mcsdk_streaming_encoder_finalize(se, OUTPUT_FILE);
                }
            }
            mcsdk_audio_buffer_release(buffer);
        }
        mcsdk_decoder_release(decoder);
        mcsdk_encoder_release(encoder);
    }
    mcsdk_data_stream_release(in);
    mcsdk_data_stream_release(out);
    printf("test_decode_encode: done.\n");
}

static void test_low_level_playback() {
    mcsdk_audio_player_gain gain = mcsdk_audio_player_get_default_gain();
    mcsdk_audio_output output = mcsdk_env_get_default_output();
    mcsdk_audio_player player = mcsdk_audio_player_create(INPUT_FILE, output, NULL, gain);
    mcsdk_audio_output_set_volume(output, 0.75);
    mcsdk_audio_output_resume(output);
    mcsdk_audio_player_play(player);
    printf("test_low_level_playback: playing for 5 seconds...\n");
    for (int i = 0; i < 5; i++) {
        internal_sleep(1);
        printf(" %d\n", i + 1);
    }
    mcsdk_audio_player_release(player, mcsdk_audio_player_release_mode_no_drain);
    mcsdk_audio_output_release(output);
    printf("test_low_level_playback: done.\n");
}

static void test_high_level_playback(mcsdk_context* context) {
    printf("test_playback: loading 'a day in the life' tracks\n");
    mcsdk_track_list tl = mcsdk_svc_metadata_query_tracks(
        context->metadata, "a day in the life", mcsdk_no_limit, mcsdk_no_offset);
    // mcsdk_track_list tl = mcsdk_track_list_create(context);
    // mcsdk_track_list_editor tle = mcsdk_track_list_edit(tl);
    // mcsdk_track_list_editor_add(tle, 4132LL);
    // mcsdk_track_list_editor_release(tle);
    mcsdk_svc_playback_play(context->playback, tl, 0);
    mcsdk_track_list_release(tl);
    printf("test_high_level_playback: playing for 5 seconds...\n");
    for (int i = 0; i < 5; i++) {
        internal_sleep(1);
        printf(" %d\n", i + 1);
    }
    mcsdk_svc_playback_stop(context->playback);
    printf("test_high_level_playback: done.\n");
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

static void configure_environment() {
    const char* suffix = "stderr.log";
    char* dest_path = NULL;
    int length = mcsdk_env_get_path(mcsdk_path_type_data, NULL, 0) + strlen(suffix);
    dest_path = malloc(length);
    mcsdk_env_get_path(mcsdk_path_type_data, dest_path, length);
    strncat(dest_path, suffix, length);
    freopen(dest_path, "w", stderr);
    printf("stderr will be written to %s", dest_path);
    free(dest_path);
}

int main(int argc, char** argv) {
    configure_environment();
    mcsdk_context* context = NULL;
    mcsdk_context_init(&context);
    if (context) {
        printf("main: context initialized\n");
        test_metadata(context);
        test_low_level_playback();
        test_high_level_playback(context);
        test_decode_encode();
        mcsdk_context_release(&context);
        printf("main: context released\n");
    }
    return 0;
}
