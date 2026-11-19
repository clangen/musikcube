#include <musikcore/musikcore.h>
#include <sqlite3.h>

#include <stdio.h>
#include <string.h>

void MC_CALL query_test_wakeup(void *data);
int query_test_wait_for_wakeup(unsigned count);
void query_test_release_wakeup(void);

struct completion {
    unsigned calls;
    mc_result result;
    mc_track_page *page;
};

static void MC_CALL completed(mc_result result, mc_track_page *page, const mc_error *error, void *data) {
    struct completion *state = data;
    ++state->calls;
    state->result = error ? mc_error_get_code(error) : result;
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
            "DROP TABLE IF EXISTS track_meta; DROP TABLE IF EXISTS meta_values; DROP TABLE IF EXISTS meta_keys;"
            "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, visible INTEGER, duration INTEGER,"
            " visual_artist_id INTEGER);"
            "INSERT INTO tracks VALUES (12, 'Live% Two', 1, 42, 2), (7, 'Live% One', 1, 1, 0),"
            "(9, 'Live% Hidden', 0, 1, 0), (10, 'Live Other', 1, 1, 0);"
            "CREATE TABLE artists (id INTEGER PRIMARY KEY, name TEXT);"
            "INSERT INTO artists VALUES (2, 'The Band');"
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

static int check_metadata(const mc_track_page *page) {
    const mc_string_view empty = mc_track_page_get_string(page, 4, 4);
    return !empty.data && mc_track_page_get_count(page) == 1 &&
        mc_track_page_get_id(page, 0) == 12 && mc_track_page_get_field_count(page) == 7 &&
        equals(mc_track_page_get_field_name(page, 0), "mood") &&
        equals(mc_track_page_get_field_name(page, 1), "title") &&
        mc_track_page_get_value_count(page, 0, 0) == 2 &&
        equals(mc_track_page_get_string(page, 0, 0), "Bright") &&
        equals(mc_track_page_get_string_at(page, 0, 0, 1), "Calm") &&
        !mc_track_page_get_string_at(page, 0, 0, 2).data &&
        equals(mc_track_page_get_string(page, 0, 1), "Live% Two") &&
        equals(mc_track_page_get_string(page, 0, 2), "42") &&
        equals(mc_track_page_get_string(page, 0, 3), "The Band") &&
        mc_track_page_get_value_count(page, 0, 4) == 1 &&
        equals(mc_track_page_get_string(page, 0, 4), "") &&
        mc_track_page_get_value_count(page, 0, 5) == 0 &&
        !mc_track_page_get_string(page, 0, 5).data &&
        equals(mc_track_page_get_string(page, 0, 6), "value");
}

static int check_indexed_library(const char *directory) {
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.data_directory.data = directory;
    options.data_directory.length = strlen(directory);
    options.wakeup = query_test_wakeup;

    mc_context *context = NULL;
    if (mc_context_create(&options, &context, NULL) != MC_OK) return 1;

    const mc_string_view fields[] = {
        { "title", 5 }, { "artist", 6 }, { "genre", 5 },
        { "composer", 8 }, { "writer", 6 }, { "language", 8 }
    };
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;
    query.search.data = "Morning Light";
    query.search.length = sizeof("Morning Light") - 1;
    query.fields = fields;
    query.field_count = sizeof(fields) / sizeof(fields[0]);

    struct completion result = { 0 };
    mc_request_id request = 0;
    const int submitted = mc_library_query_tracks_async(context, &query, completed, &result, &request) == MC_OK;
    const int ready = submitted && query_test_wait_for_wakeup(1);
    query_test_release_wakeup();
    const int dispatched = ready && mc_context_dispatch(context) == MC_OK;
    mc_context_destroy(&context);

    /* Check both indexed core fields and tags read from the generated media. */
    const mc_track_page *page = result.page;
    const mc_string_view composer_a = mc_track_page_get_string_at(page, 0, 3, 0);
    const mc_string_view composer_b = mc_track_page_get_string_at(page, 0, 3, 1);
    const int composers_match = (equals(composer_a, "Avery Lane") && equals(composer_b, "Casey Reed")) ||
        (equals(composer_b, "Avery Lane") && equals(composer_a, "Casey Reed"));
    const int passed = dispatched && !context && result.calls == 1 && result.result == MC_OK &&
        mc_track_page_get_count(page) == 1 &&
        equals(mc_track_page_get_string(page, 0, 0), "Morning Light") &&
        equals(mc_track_page_get_string(page, 0, 1), "Fixture Artist") &&
        equals(mc_track_page_get_string(page, 0, 2), "Ambient") &&
        mc_track_page_get_value_count(page, 0, 3) == 2 && composers_match &&
        mc_track_page_get_value_count(page, 0, 4) == 0 &&
        equals(mc_track_page_get_string(page, 0, 5), "eng");
    mc_track_page_destroy(&result.page);
    if (!passed) fputs("indexed track query failed\n", stderr);
    return passed ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--indexed") == 0) return check_indexed_library(argv[2]);
    if (argc != 2 || !prepare_fixture(argv[1])) return 1;

    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.data_directory.data = argv[1];
    options.data_directory.length = strlen(argv[1]);
    options.wakeup = query_test_wakeup;
    mc_context *context = NULL;
    if (mc_context_create(&options, &context, NULL) != MC_OK) return 1;

    struct completion found = { 0 };
    struct completion cancelled = { 0 };
    struct completion warmup = { 0 };
    mc_request_id initial = 0;
    mc_request_id first = 0;
    mc_request_id second = 0;
    int passed = 0;
    char term[] = "Live%";
    char mood[] = "mood";
    const mc_string_view fields[] = {
        { mood, sizeof(mood) - 1 }, { "title", 5 }, { "duration", 8 },
        { "artist", 6 }, { "comment", 7 }, { "missing", 7 }, { "custom'key", 10 }
    };
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;

    /* Hold the worker in the first wakeup to control when later queries begin. */
    if (mc_library_query_tracks_async(context, &query, completed, &warmup, &initial) != MC_OK ||
        !query_test_wait_for_wakeup(1)) goto cleanup;

    query.fields = fields;
    query.field_count = sizeof(fields) / sizeof(fields[0]);
    query.search.data = term;
    query.search.length = sizeof(term) - 1;
    query.offset = 1;
    query.limit = 1;

    /* Mutate the input after submission to check the worker uses its own copy. */
    if (mc_library_query_tracks_async(context, &query, completed, &found, &first) != MC_OK) goto cleanup;
    memset(term, 'x', sizeof(term) - 1);
    memset(mood, 'x', sizeof(mood) - 1);

    /* Cancel a queued query and drain the current batch before releasing the worker. */
    if (mc_library_query_tracks_async(context, &query, completed, &cancelled, &second) != MC_OK ||
        !first || !second || first == second ||
        mc_request_cancel(context, second) != MC_OK || cancelled.calls) goto cleanup;
    if (mc_context_dispatch(context) != MC_OK ||
        warmup.calls != 1 || warmup.result != MC_OK || !warmup.page || found.calls ||
        cancelled.calls != 1 || cancelled.result != MC_ERROR_CANCELLED || cancelled.page) goto cleanup;
    query_test_release_wakeup();
    if (!query_test_wait_for_wakeup(2) || mc_context_dispatch(context) != MC_OK ||
        found.calls != 1 || found.result != MC_OK || !found.page) goto cleanup;

    /* The owned page outlives the request and its context. */
    mc_context_destroy(&context);
    passed = !context && check_metadata(found.page);

cleanup:
    query_test_release_wakeup();
    mc_context_destroy(&context);
    mc_track_page_destroy(&found.page);
    mc_track_page_destroy(&cancelled.page);
    mc_track_page_destroy(&warmup.page);
    if (passed && !found.page && !cancelled.page && !warmup.page) return 0;
    fputs("track query contract failed\n", stderr);
    return 1;
}
