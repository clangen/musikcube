#include <musikcore/musikcore.h>
#include "../../support/check.h"
#include <stdio.h>
#include <string.h>

/* Preserve the fixture's cleanup path while sharing C/C++ failure diagnostics. */
#define CHECK(expression) do { \
    if (!TEST_CHECK(expression)) { \
        goto cleanup; \
    } \
} while (0)
#define REJECT(expression, expected) do { request = 999; \
    CHECK((expression) == (expected)); CHECK(request == 0); } while (0)

static unsigned callbacks;
/* Record any unexpected page completion and release its transferred payload. */
static void MC_CALL page_done(mc_result status, mc_track_page* page, const mc_error* error, void* data) {
    (void)status; (void)error; (void)data; ++callbacks; mc_track_page_destroy(&page);
}
/* Record any unexpected result completion and release its transferred payload. */
static void MC_CALL result_done(mc_result status, mc_track_result* result, const mc_error* error, void* data) {
    (void)status; (void)error; (void)data; ++callbacks; mc_track_result_destroy(&result);
}
/* Count list completions; rejected submissions must never reach this callback. */
static void MC_CALL list_done(mc_result status, const mc_playlist_entry* entries, size_t count,
    const mc_error* error, void* data) {
    (void)status; (void)entries; (void)count; (void)error; (void)data; ++callbacks;
}
/* Count write completions; rejected submissions must never reach this callback. */
static void MC_CALL write_done(mc_result status, int64_t id, const mc_error* error, void* data) {
    (void)status; (void)id; (void)error; (void)data; ++callbacks;
}

/* Verify malformed inputs reject synchronously, clear request IDs, and never deliver callbacks. */
int main(void) {
    int passed = 0;
    mc_context* context = NULL;
    mc_library* library = NULL;
    mc_library* remote = NULL;
    mc_track_result* tracks = NULL;
    mc_track_result* remote_tracks = NULL;
    mc_track_ref* track = NULL;
    mc_request_id request = 0;
    mc_library_options options = MC_LIBRARY_OPTIONS_DEFAULT;
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;
    mc_track_search search = MC_TRACK_SEARCH_DEFAULT;
    mc_track_range range = MC_TRACK_RANGE_DEFAULT;
    mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
    mc_play_queue* queue = NULL;
    const mc_string_view name = {"name", 4};
    const char embedded_nul[] = {'x', 0, 'y'};
    const mc_string_view malformed = {embedded_nul, sizeof(embedded_nul)};
    const mc_string_view oversized = {"x", SIZE_MAX};
    const int64_t ids[] = {1, 2, 1};
    const int64_t invalid_ids[] = {1, -2};
    size_t count = 99;

    mc_context_options runtime_options = MC_CONTEXT_OPTIONS_DEFAULT;
    runtime_options.services = MC_SERVICE_ALL;
    CHECK(mc_context_create(&runtime_options, &context, NULL) == MC_ERROR_UNSUPPORTED && !context);

    /* Null lifecycle inputs and the reserved zero request ID must fail before
       touching thread-binding or request-registry state. */
    CHECK(mc_context_create(NULL, &context, NULL) == MC_ERROR_UNSUPPORTED && !context);
    /* a full zero-filled struct explicitly opts out of services; unlike a
    missing options struct it must stay usable in this ABI-only build. */
    mc_context_options dispatcher_options = { 0 };
    dispatcher_options.struct_size = sizeof(dispatcher_options);
    CHECK(mc_context_create(&dispatcher_options, &context, NULL) == MC_OK);
    /* immutable service discovery validates both arguments and reports the
    explicit no-services configuration without starting any production state. */
    mc_services services = UINT32_MAX;
    CHECK(mc_context_get_services(NULL, &services) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(services == UINT32_MAX);
    CHECK(mc_context_get_services(context, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_get_services(context, &services) == MC_OK && services == MC_SERVICE_NONE);
    /* URI resources require providers and preserve output slots on rejection. */
    mc_player* uri_player = NULL;
    CHECK(mc_player_create(context, &uri_player) == MC_ERROR_UNSUPPORTED && !uri_player);
    CHECK(mc_player_create(NULL, &uri_player) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_player_destroy(&uri_player) == MC_OK && !uri_player);
    CHECK(mc_player_play_uri(NULL, name) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_player_pause(NULL) == MC_ERROR_INVALID_ARGUMENT);
    mc_player_snapshot uri_snapshot = MC_PLAYER_SNAPSHOT_DEFAULT;
    CHECK(mc_player_get_snapshot(NULL, &uri_snapshot) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(uri_snapshot.state == MC_PLAYER_STOPPED);
    mc_playback* playback = NULL;
    CHECK(mc_context_get_playback(context, &playback) == MC_ERROR_UNSUPPORTED && !playback);
    CHECK(mc_context_select_library(context, MC_LIBRARY_LOCAL) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_context_select_library(context, MC_LIBRARY_NONE) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_read_library_status(context, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_set_library_changed_callback(NULL, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_set_playlist_changed_callback(NULL, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_set_equalizer_changed_callback(NULL, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_reconnect_remote_library(context) == MC_ERROR_UNSUPPORTED);
    mc_indexer_state indexer_state = MC_INDEXER_IDLE;
    CHECK(mc_indexer_get_state(context, &indexer_state) == MC_ERROR_UNSUPPORTED);
    CHECK(indexer_state == MC_INDEXER_IDLE);
    CHECK(mc_indexer_schedule(context, MC_INDEXER_REBUILD + 1) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_output_read_configuration(NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_output_read_driver_configuration(name, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_output_configure(name, malformed) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_output_select_driver(name) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_output_select_device(name, malformed) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_output_set_transport(MC_OUTPUT_GAPLESS) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_output_set_transport((mc_output_transport) -1) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_get_gain_settings(NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_set_preamp(20.0) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_playback_set_preamp(-21.0) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_set_replay_gain(MC_REPLAY_GAIN_TRACK) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_playback_set_replay_gain((mc_replay_gain_mode) -1) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_visualizer_list(NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_visualizer_show(0) == MC_ERROR_NOT_FOUND);
    CHECK(mc_visualizer_hide() == MC_ERROR_NOT_FOUND);
    const mc_setting_entry setting = { { "test", 4 }, { MC_SETTING_BOOLEAN, { false } } };
    CHECK(mc_settings_write(MC_SETTINGS_APPLICATION, NULL, 1) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_settings_write(MC_SETTINGS_APPLICATION, NULL, 0) == MC_OK);
    CHECK(mc_settings_write(MC_SETTINGS_APPLICATION, &setting, 1) == MC_ERROR_UNSUPPORTED);
    CHECK(mc_settings_write(MC_SETTINGS_APPLICATION, &setting, SIZE_MAX) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_plugins_list(NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_plugin_set_enabled(name, false) == MC_ERROR_NOT_FOUND);
    CHECK(mc_plugin_write_setting(name, &setting) == MC_ERROR_NOT_FOUND);
    mc_equalizer_state equalizer = MC_EQUALIZER_STATE_DEFAULT;
    CHECK(mc_equalizer_get_state(NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_equalizer_get_state(&equalizer) == MC_ERROR_UNSUPPORTED);
    CHECK(equalizer.struct_size == sizeof(equalizer) && !equalizer.enabled);
    double gains[MC_EQUALIZER_BAND_COUNT] = { 0 };
    CHECK(mc_equalizer_set_gains(gains, MC_EQUALIZER_BAND_COUNT) == MC_ERROR_UNSUPPORTED);
    gains[0] = 21;
    CHECK(mc_equalizer_set_gains(gains, MC_EQUALIZER_BAND_COUNT) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_dispatch(NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_context_shutdown(NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_request_cancel(NULL, 1) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_request_cancel(context, 0) == MC_ERROR_INVALID_ARGUMENT);
    /* Impossible lengths and embedded NUL must be rejected before copying;
       backend path handling must not interpret a different string than C saw. */
    CHECK(mc_library_create(NULL, &library) == MC_ERROR_INVALID_ARGUMENT);
    options.locator = oversized;
    CHECK(mc_library_create(&options, &library) == MC_ERROR_INVALID_ARGUMENT && !library);
    options.locator = malformed;
    CHECK(mc_library_create(&options, &library) == MC_ERROR_INVALID_ARGUMENT && !library);
    options.locator = (mc_string_view){".", 1};
    CHECK(mc_library_create(&options, &library) == MC_OK);
    options.kind = MC_LIBRARY_REMOTE;
    CHECK(mc_library_create(&options, &remote) == MC_OK);
    /* Construct identities without opening a database: malformed admission
       should be rejected independently of filesystem/backend availability. */
    CHECK(mc_library_make_track_result(library, ids, 3, &tracks) == MC_OK);
    CHECK(mc_track_result_get_ref(tracks, 0, &track) == MC_OK);
    CHECK(mc_library_make_track_result(remote, ids, 3, &remote_tracks) == MC_OK);

    /* Each required input is checked independently. Rejection must reset an
       existing request value so callers cannot accidentally cancel older work. */
    REJECT(mc_library_query_tracks_async(NULL, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_query_tracks_async(context, NULL, &query, page_done, NULL, &request), MC_ERROR_NOT_FOUND);
    REJECT(mc_library_query_tracks_async(context, library, NULL, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_query_tracks_async(context, library, &query, NULL, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_library_query_tracks_async(context, library, &query, page_done, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    /* Invalid text, reserved flags, and absent field arrays must not reach a
       worker that could dereference caller storage after this function returns. */
    query.search = oversized;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    query.search = malformed;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    query.search = (mc_string_view){NULL, 0};
    query.flags = 1;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_UNSUPPORTED);
    query.flags = 0;
    query.fields = &oversized; query.field_count = 1;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    query.fields = NULL;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    /* Offsets, limits, and missing struct prefixes exercise integer-boundary
       validation before allocation or narrowing to SQLite's range types. */
    query.field_count = 0; query.offset = SIZE_MAX;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    query.offset = 0; query.limit = SIZE_MAX;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    query.limit = 0; query.struct_size = 0;
    REJECT(mc_library_query_tracks_async(context, library, &query, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);

    /* Search/selection entry points must share text/enum validation rather
       than admitting inputs that only one backend happens to understand. */
    search.text = oversized;
    REJECT(mc_library_open_track_search_async(context, library, &search, result_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    search.text = name; search.order = UINT32_MAX;
    REJECT(mc_library_open_track_search_async(context, library, &search, result_done, NULL, &request), MC_ERROR_UNSUPPORTED);
    REJECT(mc_library_open_category_async(context, library, oversized, 1, MC_TRACK_SEARCH_ALBUM, result_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_open_directory_async(context, library, malformed, result_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_open_playlist_async(context, library, 0, result_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    /* An unbound identity has no playlist backend; invalid counts/IDs must
       still be diagnosed before that unsupported-backend path. */
    REJECT(mc_library_list_playlists_async(context, library, list_done, NULL, &request), MC_ERROR_UNSUPPORTED);
    REJECT(mc_library_save_playlist_async(context, NULL, 0, name, ids, 2, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_append_playlist_async(context, NULL, 1, ids, 2, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_rename_playlist_async(context, NULL, 1, name, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_delete_playlist_async(context, NULL, 1, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_save_playlist_async(context, library, 0, name, ids, SIZE_MAX, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_save_playlist_async(context, library, 0, name, invalid_ids, 2, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_append_playlist_async(context, library, 1, ids, SIZE_MAX, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_rename_playlist_async(context, library, 1, oversized, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    REJECT(mc_library_delete_playlist_async(context, library, -1, write_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    /* A remote identity alone does not establish a readable session. Result
       and reference reads must return UNSUPPORTED instead of dereferencing a
       nonexistent backend, even when the requested fields use defaults. */
    REJECT(mc_track_result_read_async(context, tracks, NULL, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    range.fields = &malformed; range.field_count = 1;
    REJECT(mc_track_result_read_async(context, tracks, &range, page_done, NULL, &request), MC_ERROR_INVALID_ARGUMENT);
    range.fields = NULL; range.field_count = 0;
    REJECT(mc_track_result_read_async(context, remote_tracks, &range, page_done, NULL, &request), MC_ERROR_UNSUPPORTED);
    mc_track_ref_destroy(&track);
    CHECK(mc_track_result_get_ref(remote_tracks, 0, &track) == MC_OK);
    REJECT(mc_track_ref_read_async(context, track, NULL, 0, page_done, NULL, &request), MC_ERROR_UNSUPPORTED);

    /* Standalone consumers may have no playback session. Every command must
       reject null safely rather than assuming an app-created engine exists. */
    CHECK(mc_playback_get_snapshot(NULL, &snapshot) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_get_current_track(NULL, &track) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_get_current_track_metadata(NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_set_changed_callback(NULL, NULL, NULL, NULL, 0) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_get_queue(NULL, &queue) == MC_ERROR_INVALID_ARGUMENT && !queue);
    CHECK(mc_playback_control(NULL, MC_PLAYBACK_STOP, 0) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_play_result(NULL, tracks, 0) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_use_track(NULL, track, MC_PLAYBACK_TRACK_APPEND) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_replace_queue(NULL, tracks) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_play_queue_row(NULL, NULL, 0) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_queue_move(NULL, NULL, 0, 1) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_queue_remove(NULL, NULL, 0) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_queue_clear(NULL) == MC_ERROR_INVALID_ARGUMENT);
    CHECK(mc_playback_set_queue_changed_callback(NULL, NULL, NULL) == MC_ERROR_INVALID_ARGUMENT);
    /* Read-only accessors deliberately support null handles with consistent
       empty sentinels, allowing bindings to inspect absent owned values safely. */
    CHECK(!mc_play_queue_get_ids(NULL, &count) && count == 0);
    CHECK(mc_play_queue_get_current_row(NULL) == SIZE_MAX && mc_play_queue_get_retained_row(NULL) == SIZE_MAX);
    CHECK(!mc_track_page_get_count(NULL) && !mc_track_page_get_id(NULL, 0) && !mc_track_page_get_field_count(NULL));
    CHECK(!mc_track_page_get_field_name(NULL, 0).data && !mc_track_page_get_string(NULL, 0, 0).data);
    CHECK(!mc_track_page_get_value_count(NULL, 0, 0) && !mc_track_page_get_string_at(NULL, 0, 0, 0).data);
    /* No rejected operation may leave a hidden accepted request: dispatching
       the context must not invoke any of the counting callbacks above. */
    CHECK(mc_context_dispatch(context) == MC_OK && callbacks == 0);
    passed = 1;
cleanup:
    mc_context_destroy(&context);
    mc_track_ref_destroy(&track);
    mc_track_result_destroy(&tracks);
    mc_track_result_destroy(&remote_tracks);
    mc_library_destroy(&library);
    mc_library_destroy(&remote);
    return passed ? 0 : 1;
}
