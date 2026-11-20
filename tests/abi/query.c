#include <musikcore/library.h>
#include <sqlite3.h>

#include <stdio.h>
#include <string.h>

void MC_CALL query_test_wakeup(void *data);
int query_test_wait_for_wakeup(unsigned count);
void query_test_release_wakeup(void);

#define STRING_VIEW(text) { (text), sizeof(text) - 1 }
#define MOOD_KEY "mood"
#define COMMENT_KEY "comment"
#define MISSING_KEY "missing"
#define CUSTOM_KEY "custom'key"
enum { fixture_band_track = 12 };

enum fixture_field {
    fixture_mood,
    fixture_title,
    fixture_duration,
    fixture_artist,
    fixture_comment,
    fixture_missing,
    fixture_custom,
    fixture_field_count
};

enum indexed_field {
    indexed_title,
    indexed_artist,
    indexed_genre,
    indexed_composer,
    indexed_writer,
    indexed_language,
    indexed_field_count
};

static const char indexed_title_value[] = "Morning Light";

struct completion {
    unsigned calls;
    mc_result result;
    mc_track_page *page;
};

static void MC_CALL completed(mc_result result, mc_track_page *page, const mc_error *error, void *data) {
    struct completion *state = data;
    ++state->calls;
    state->result = result == MC_OK ? (error ? MC_ERROR_INTERNAL : MC_OK)
        : (error && mc_error_get_code(error) == result && mc_error_get_message(error).length
            ? result : MC_ERROR_INTERNAL);
    state->page = page;
}

static int prepare_fixture(const char *root) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/musik.db", root);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;

    sqlite3 *database = NULL;
    int result = sqlite3_open(path, &database);
    if (result == SQLITE_OK) {
        result = sqlite3_exec(database,
            "DROP TABLE IF EXISTS tracks; DROP TABLE IF EXISTS artists;"
            "DROP TABLE IF EXISTS albums; DROP TABLE IF EXISTS genres;"
            "DROP TABLE IF EXISTS track_meta; DROP TABLE IF EXISTS meta_values; DROP TABLE IF EXISTS meta_keys;"
            "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, visible INTEGER, duration INTEGER,"
            " visual_artist_id INTEGER, album_id INTEGER DEFAULT 1, visual_genre_id INTEGER DEFAULT 1,"
            " disc INTEGER DEFAULT 1, track INTEGER DEFAULT 1);"
            "INSERT INTO tracks (id, title, visible, duration, visual_artist_id)"
            " VALUES (12, 'Live% Two', 1, 42, 2), (7, 'Live% One', 1, 1, 0),"
            "(9, 'Live% Hidden', 0, 1, 0), (10, 'Live Other', 1, 1, 0);"
            "CREATE TABLE artists (id INTEGER PRIMARY KEY, name TEXT);"
            "INSERT INTO artists VALUES (2, 'The Band');"
            "CREATE TABLE albums (id INTEGER PRIMARY KEY, name TEXT);"
            "INSERT INTO albums VALUES (1, 'Album');"
            "CREATE TABLE genres (id INTEGER PRIMARY KEY, name TEXT);"
            "INSERT INTO genres VALUES (1, 'Genre');"
            "CREATE TABLE meta_keys (id INTEGER PRIMARY KEY, name TEXT);"
            "CREATE TABLE meta_values (id INTEGER PRIMARY KEY, meta_key_id INTEGER, sort_order INTEGER, content TEXT);"
            "CREATE TABLE track_meta (track_id INTEGER, meta_value_id INTEGER);"
            "INSERT INTO meta_keys VALUES (1, 'mood'), (2, 'comment'), (3, 'custom''key');"
            "INSERT INTO meta_values VALUES (1, 1, 20, 'Calm'), (2, 1, 10, 'Bright'),"
            "(3, 2, 0, ''), (4, 3, 0, 'value');"
            "INSERT INTO track_meta VALUES (7, 1), (7, 2), (12, 1), (12, 2), (12, 3), (12, 4);",
            NULL, NULL, NULL);
    }
    sqlite3_close(database);
    return result == SQLITE_OK;
}

static int equals(mc_string_view view, const char *value) {
    return view.data && view.length == strlen(value) && memcmp(view.data, value, view.length) == 0;
}

static void MC_CALL search_completed(mc_result result, mc_track_result *tracks, const mc_error *error, void *data) {
    (void)result;
    (void)error;
    *(mc_track_result **)data = tracks;
}

static int hide_fixture_track(const char *root) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/musik.db", root);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    const int ok = sqlite3_open(path, &database) == SQLITE_OK &&
        sqlite3_prepare_v2(database, "UPDATE tracks SET visible = 0 WHERE id = ?1", -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_bind_int(statement, 1, fixture_band_track) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return ok;
}

static mc_library *open_local_library(const char *directory) {
    mc_library_options options = MC_LIBRARY_OPTIONS_DEFAULT;
    options.locator = (mc_string_view){ directory, strlen(directory) };
    mc_library *library = NULL;
    mc_library_create(&options, &library);
    return library;
}

static int check_stable_range(mc_context *context, const mc_library *library, const char *directory) {
    mc_track_search search = MC_TRACK_SEARCH_DEFAULT;
    search.text = (mc_string_view)STRING_VIEW("The Band");
    mc_track_result *tracks = NULL;
    mc_track_ref *track = NULL;
    struct completion range = { 0 };
    struct completion resolved = { 0 };
    mc_request_id request = 0;
    mc_track_range options = MC_TRACK_RANGE_DEFAULT;
    int passed = 0;

    if (mc_library_open_track_search_async(context, library, &search, search_completed, &tracks, &request) != MC_OK ||
        !query_test_wait_for_wakeup(3) || mc_context_dispatch(context) != MC_OK ||
        mc_track_result_get_count(tracks) != 1 || mc_track_result_get_id(tracks, 0) != fixture_band_track ||
        !mc_track_result_starts_album(tracks, 0) ||
        mc_track_result_get_ref(tracks, 0, &track) != MC_OK ||
        mc_track_ref_get_id(track) != fixture_band_track || !hide_fixture_track(directory)) goto cleanup;

    /* An index update removes metadata without shifting the old result's positions. */
    if (mc_track_result_read_async(context, tracks, &options, completed, &range, &request) != MC_OK ||
        !query_test_wait_for_wakeup(4) || mc_context_dispatch(context) != MC_OK) goto cleanup;
    if (mc_track_ref_read_async(context, track, NULL, 0, completed, &resolved, &request) != MC_OK ||
        !query_test_wait_for_wakeup(5) || mc_context_dispatch(context) != MC_OK) goto cleanup;
    passed = resolved.calls == 1 && resolved.result == MC_ERROR_NOT_FOUND && !resolved.page &&
        range.calls == 1 && range.result == MC_OK && mc_track_page_get_count(range.page) == 1 &&
        mc_track_page_get_id(range.page, 0) == fixture_band_track &&
        mc_track_page_get_value_count(range.page, 0, 0) == 0;

cleanup:
    mc_context_shutdown(context);
    mc_track_result_destroy(&tracks);
    mc_track_ref_destroy(&track);
    mc_track_page_destroy(&range.page);
    mc_track_page_destroy(&resolved.page);
    return passed;
}

static int check_metadata(const mc_track_page *page) {
    const mc_string_view empty = mc_track_page_get_string(page, mc_track_page_get_count(page), fixture_comment);
    return !empty.data && mc_track_page_get_count(page) == 1 &&
        mc_track_page_get_id(page, 0) == 12 && mc_track_page_get_field_count(page) == fixture_field_count &&
        equals(mc_track_page_get_field_name(page, fixture_mood), MOOD_KEY) &&
        equals(mc_track_page_get_field_name(page, fixture_title), MC_TRACK_FIELD_TITLE) &&
        mc_track_page_get_value_count(page, 0, fixture_mood) == 2 &&
        equals(mc_track_page_get_string(page, 0, fixture_mood), "Bright") &&
        equals(mc_track_page_get_string_at(page, 0, fixture_mood, 1), "Calm") &&
        !mc_track_page_get_string_at(page, 0, fixture_mood, 2).data &&
        equals(mc_track_page_get_string(page, 0, fixture_title), "Live% Two") &&
        equals(mc_track_page_get_string(page, 0, fixture_duration), "42") &&
        equals(mc_track_page_get_string(page, 0, fixture_artist), "The Band") &&
        mc_track_page_get_value_count(page, 0, fixture_comment) == 1 &&
        equals(mc_track_page_get_string(page, 0, fixture_comment), "") &&
        mc_track_page_get_value_count(page, 0, fixture_missing) == 0 &&
        !mc_track_page_get_string(page, 0, fixture_missing).data &&
        equals(mc_track_page_get_string(page, 0, fixture_custom), "value");
}

static int check_indexed_library(const char *directory) {
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.wakeup = query_test_wakeup;
    options.track_query_default_limit = -1;
    options.track_query_max_fields = -1;

    mc_context *context = NULL;
    if (mc_context_create(&options, &context, NULL) != MC_OK) return 1;
    mc_library *library = open_local_library(directory);
    if (!library) {
        mc_context_destroy(&context);
        return 1;
    }

    const mc_string_view fields[indexed_field_count] = {
        [indexed_title] = STRING_VIEW(MC_TRACK_FIELD_TITLE),
        [indexed_artist] = STRING_VIEW(MC_TRACK_FIELD_ARTIST),
        [indexed_genre] = STRING_VIEW(MC_TRACK_FIELD_GENRE),
        [indexed_composer] = STRING_VIEW("composer"),
        [indexed_writer] = STRING_VIEW("writer"),
        [indexed_language] = STRING_VIEW("language"),
    };
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;
    query.search.data = indexed_title_value;
    query.search.length = sizeof(indexed_title_value) - 1;
    query.fields = fields;
    query.field_count = indexed_field_count;

    struct completion result = { 0 };
    mc_request_id request = 0;
    query.limit = MC_TRACK_QUERY_MAX_LIMIT + 1;
    const int rejects_large_page = mc_library_query_tracks_async(
        context, library, &query, completed, &result, &request) == MC_ERROR_INVALID_ARGUMENT && !request;
    query.limit = 0;

    const int submitted = mc_library_query_tracks_async(context, library, &query, completed, &result, &request) == MC_OK;
    const int ready = submitted && query_test_wait_for_wakeup(1);
    query_test_release_wakeup();
    const int dispatched = ready && mc_context_dispatch(context) == MC_OK;
    mc_context_destroy(&context);

    mc_library_destroy(&library);

    /* Check both indexed core fields and tags read from the generated media. */
    const mc_track_page *page = result.page;
    const mc_string_view composer_a = mc_track_page_get_string_at(page, 0, indexed_composer, 0);
    const mc_string_view composer_b = mc_track_page_get_string_at(page, 0, indexed_composer, 1);
    const int composers_match = (equals(composer_a, "Avery Lane") && equals(composer_b, "Casey Reed")) ||
        (equals(composer_b, "Avery Lane") && equals(composer_a, "Casey Reed"));
    const int passed = rejects_large_page && dispatched && !context && result.calls == 1 && result.result == MC_OK &&
        mc_track_page_get_count(page) == 1 &&
        equals(mc_track_page_get_string(page, 0, indexed_title), indexed_title_value) &&
        equals(mc_track_page_get_string(page, 0, indexed_artist), "Fixture Artist") &&
        equals(mc_track_page_get_string(page, 0, indexed_genre), "Ambient") &&
        mc_track_page_get_value_count(page, 0, indexed_composer) == 2 && composers_match &&
        mc_track_page_get_value_count(page, 0, indexed_writer) == 0 &&
        equals(mc_track_page_get_string(page, 0, indexed_language), "eng");
    mc_track_page_destroy(&result.page);
    if (!passed) fputs("indexed track query failed\n", stderr);
    return passed ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--indexed") == 0) return check_indexed_library(argv[2]);
    if (argc != 2 || !prepare_fixture(argv[1])) return 1;

    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.wakeup = query_test_wakeup;
    options.track_query_default_limit = 3;
    options.track_query_max_limit = 2;
    options.track_query_max_fields = fixture_field_count;
    mc_context *context = NULL;
    if (mc_context_create(&options, &context, NULL) != MC_OK) return 1;

    mc_library *library = open_local_library(argv[1]);
    if (!library) {
        mc_context_destroy(&context);
        return 1;
    }

    struct completion found = { 0 };
    struct completion cancelled = { 0 };
    struct completion warmup = { 0 };
    mc_request_id initial = 0;
    mc_request_id first = 0;
    mc_request_id second = 0;
    int passed = 0;
    char term[] = "Live%";
    char mood[] = MOOD_KEY;
    const mc_string_view fields[fixture_field_count + 1] = {
        [fixture_mood] = STRING_VIEW(mood),
        [fixture_title] = STRING_VIEW(MC_TRACK_FIELD_TITLE),
        [fixture_duration] = STRING_VIEW(MC_TRACK_FIELD_DURATION),
        [fixture_artist] = STRING_VIEW(MC_TRACK_FIELD_ARTIST),
        [fixture_comment] = STRING_VIEW(COMMENT_KEY),
        [fixture_missing] = STRING_VIEW(MISSING_KEY),
        [fixture_custom] = STRING_VIEW(CUSTOM_KEY),
        [fixture_field_count] = STRING_VIEW(MC_TRACK_FIELD_ID),
    };
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;

    /* Reject requests over the context caps before reading the field array. */
    query.limit = 3;
    if (mc_library_query_tracks_async(context, library, &query, completed, &warmup, &initial) != MC_ERROR_INVALID_ARGUMENT ||
        initial) goto cleanup;
    query.limit = 0;
    query.fields = fields;
    query.field_count = fixture_field_count + 1;
    if (mc_library_query_tracks_async(context, library, &query, completed, &warmup, &initial) != MC_ERROR_INVALID_ARGUMENT ||
        initial) goto cleanup;
    query.fields = NULL;
    query.field_count = 0;

    /* Hold the worker in the first wakeup to control when later queries begin. */
    if (mc_library_query_tracks_async(context, library, &query, completed, &warmup, &initial) != MC_OK ||
        !query_test_wait_for_wakeup(1)) goto cleanup;

    query.fields = fields;
    query.field_count = fixture_field_count;
    query.search.data = term;
    query.search.length = sizeof(term) - 1;
    query.offset = 1;
    query.limit = 1;

    /* Mutate the input after submission to check the worker uses its own copy. */
    if (mc_library_query_tracks_async(context, library, &query, completed, &found, &first) != MC_OK) goto cleanup;
    memset(term, 'x', sizeof(term) - 1);
    memset(mood, 'x', sizeof(mood) - 1);

    /* Cancel a queued query and drain the current batch before releasing the worker. */
    if (mc_library_query_tracks_async(context, library, &query, completed, &cancelled, &second) != MC_OK ||
        !first || !second || first == second ||
        mc_request_cancel(context, second) != MC_OK || cancelled.calls) goto cleanup;
    if (mc_context_dispatch(context) != MC_OK ||
        warmup.calls != 1 || warmup.result != MC_OK || mc_track_page_get_count(warmup.page) != 2 || found.calls ||
        cancelled.calls != 1 || cancelled.result != MC_ERROR_CANCELLED || cancelled.page) goto cleanup;
    query_test_release_wakeup();
    if (!query_test_wait_for_wakeup(2) || mc_context_dispatch(context) != MC_OK ||
        found.calls != 1 || found.result != MC_OK || !found.page) goto cleanup;

    /* The owned page outlives the request and its context. */
    passed = check_stable_range(context, library, argv[1]);
    mc_context_destroy(&context);
    passed = passed && !context && check_metadata(found.page);

cleanup:
    query_test_release_wakeup();
    mc_context_destroy(&context);
    mc_library_destroy(&library);
    mc_track_page_destroy(&found.page);
    mc_track_page_destroy(&cancelled.page);
    mc_track_page_destroy(&warmup.page);
    if (passed && !found.page && !cancelled.page && !warmup.page) return 0;
    fputs("track query contract failed\n", stderr);
    return 1;
}
