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

static void sleep_for(const char* tag, int seconds) {
    printf("[%s] sleeping for %d seconds...\n", tag, seconds);
    for (int i = 0; i < seconds; i++) {
        internal_sleep(1);
        printf("[%s] %d\n", tag, i + 1);
    }
}

static void indexer_started_callback(mcsdk_svc_indexer in) {
    printf("[indexer_started_callback]\n");
}

static void indexer_progress_callback(mcsdk_svc_indexer in, int updated_count) {
    printf("[indexer_progress_callback] %d\n", updated_count);
}

static void indexer_finished_callback(mcsdk_svc_indexer in, int total_updated_count) {
    printf("[indexer_finished_callback] %d\n", total_updated_count);
}

static bool run_test_query_callback(mcsdk_svc_library library, mcsdk_db_connection connection, void* user_context) {
    bool result = false;
    mcsdk_db_statement stmt = mcsdk_db_statement_create(connection, "SELECT COUNT(*) FROM tracks");
    if (mcsdk_db_statement_step(stmt) == mcsdk_db_result_row) {
        int total_tracks = mcsdk_db_statement_column_int64(stmt, 0);
        printf("[run_test_query_callback] success! %d total tracks\n", total_tracks);
        result = true;
    }
    else {
        printf("[run_test_query_callback] failed\n");
    }
    mcsdk_db_statement_release(stmt);
    return result;
}

static void player_mixpoint_hit(mcsdk_audio_player p, int id, double time) {
    printf("[player_mixpoint_hit] id=%x time=%f\n", id, time);
}

static void test_indexer(mcsdk_context* context) {
    char buffer[4096];
    for (int i = 0; i < mcsdk_svc_indexer_get_paths_count(context->indexer); i++) {
        mcsdk_svc_indexer_get_paths_at(context->indexer, i, buffer, sizeof(buffer));
        printf("[test_indexer] path: %s\n", buffer);
    }
    mcsdk_svc_indexer_callbacks cb = { 0 };
    cb.on_started = indexer_started_callback;
    cb.on_progress = indexer_progress_callback;
    cb.on_finished = indexer_finished_callback;
    mcsdk_svc_indexer_add_callbacks(context->indexer, &cb);
    mcsdk_svc_indexer_schedule(context->indexer, mcsdk_svc_indexer_sync_type_local);
    sleep_for("test_indexer", 5);
    mcsdk_svc_indexer_remove_callbacks(context->indexer, &cb);
}

static void test_decode_encode() {
    mcsdk_data_stream in = mcsdk_env_open_data_stream(INPUT_FILE, mcsdk_stream_open_flags_read);
    mcsdk_data_stream out = mcsdk_env_open_data_stream(OUTPUT_FILE, mcsdk_stream_open_flags_write);
    if (mcsdk_handle_ok(in) && mcsdk_handle_ok(out)) {
        printf("[test_decode_encode] decoding from %s\n", INPUT_FILE);
        printf("[test_decode_encode] encoding to %s\n", OUTPUT_FILE);
        mcsdk_decoder decoder = mcsdk_env_open_decoder(in);
        mcsdk_encoder encoder = mcsdk_env_open_encoder(ENCODER_TYPE);
        if (mcsdk_handle_ok(decoder) && mcsdk_handle_ok(encoder)) {
            printf("[test_decode_encode] encoder and decoder opened successfully. running...\n");
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
    printf("[test_decode_encode] done.\n");
}

static void test_low_level_playback() {
    mcsdk_audio_player_callbacks cbs = { 0 };
    cbs.on_mixpoint = player_mixpoint_hit;
    mcsdk_audio_player_gain gain = mcsdk_audio_player_get_default_gain();
    mcsdk_audio_output output = mcsdk_env_get_default_output();
    mcsdk_audio_player player = mcsdk_audio_player_create(INPUT_FILE, output, &cbs, gain);
    mcsdk_audio_player_add_mix_point(player, 0xdeadbeef, 1.25);
    mcsdk_audio_output_set_volume(output, 0.75);
    mcsdk_audio_output_resume(output);
    mcsdk_audio_player_play(player);
    printf("[test_low_level_playback] playing for 5 seconds...\n");
    sleep_for("test_low_level_playback", 5);
    mcsdk_audio_player_release(player, mcsdk_audio_player_release_mode_no_drain);
    mcsdk_audio_output_release(output);
    printf("[test_low_level_playback] done.\n");
}

static void test_high_level_playback(mcsdk_context* context) {
    printf("[test_playback] loading 'a day in the life' tracks\n");
    mcsdk_track_list tl = mcsdk_svc_metadata_query_tracks(
        context->metadata, "a day in the life", mcsdk_no_limit, mcsdk_no_offset);
    // mcsdk_track_list tl = mcsdk_track_list_create(context);
    // mcsdk_track_list_editor tle = mcsdk_track_list_edit(tl);
    // mcsdk_track_list_editor_add(tle, 4132LL);
    // mcsdk_track_list_editor_release(tle);
    mcsdk_svc_playback_play(context->playback, tl, 0);
    mcsdk_track_list_release(tl);
    printf("[test_high_level_playback] playing for 5 seconds...\n");
    sleep_for("test_high_level_playback", 5);
    mcsdk_svc_playback_stop(context->playback);
    printf("[test_high_level_playback] done.\n");
}

static void test_metadata(mcsdk_context* context) {
    printf("[test_metadata] querying 'beatles' albums\n");
    mcsdk_map_list ml = mcsdk_svc_metadata_query_albums(context->metadata, "beatles");
    int count = mcsdk_map_list_get_count(ml);
    for (int i = 0; i < count; i++) {
        mcsdk_map m = mcsdk_map_list_get_at(ml, i);
        size_t len = mcsdk_map_get_string(m, "album", NULL, 0);
        char* str = malloc(len * sizeof(char));
        mcsdk_map_get_string(m, "album", str, len);
        printf("[test_metadata] album: %s\n", str);
        free(str);
    }
    mcsdk_map_list_release(ml);
    printf("[test_metadata] done.\n");
}

static void test_library(mcsdk_context* context) {
    mcsdk_svc_library_run_query(
        context->library,
        "test query",
        NULL,
        run_test_query_callback,
        mcsdk_svc_library_query_flag_synchronous);
}

static void configure_stderr() {
    const char* suffix = "stderr.log";
    char* dest_path = NULL;
    int length = mcsdk_env_get_path(mcsdk_path_type_data, NULL, 0) + strlen(suffix);
    dest_path = malloc(length);
    mcsdk_env_get_path(mcsdk_path_type_data, dest_path, length);
    strncat(dest_path, suffix, length);
    freopen(dest_path, "w", stderr);
    printf("[configure_stderr] stderr will be written to %s\n", dest_path);
    free(dest_path);
}

int main(int argc, char** argv) {
    configure_stderr();
    mcsdk_env_init();
    mcsdk_context* context = NULL;
    mcsdk_context_init(&context);
    if (context) {
        printf("[main] context initialized\n");
        test_library(context);
        test_indexer(context);
        test_metadata(context);
        test_low_level_playback();
        test_high_level_playback(context);
        test_decode_encode();
        mcsdk_context_release(&context);
        printf("[main] context released\n");
    }
    mcsdk_env_release();
    return 0;
}
