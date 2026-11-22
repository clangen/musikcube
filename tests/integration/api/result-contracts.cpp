#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include <sqlite3.h>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

void TestFailNextAllocation();

namespace {
    namespace api = musik::core::api;
    const std::string unicode = "音楽 — Björk 🎵";
    const std::string longTitle(65536, 'x');

    /* Keep edge metadata and ownership transitions in one scenario so truncation, shifted rows, and
       dangling views are caught at the public boundary. */
    bool check(const char* root) {
        /* SQL fixture setup deliberately bypasses indexing: API readers must
           preserve values that ordinary tag readers might normalize first. */
        sqlite3* raw = nullptr;
        if (sqlite3_open((std::string(root) + "/musik.db").c_str(), &raw) != SQLITE_OK) return false;
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        if (sqlite3_exec(raw,
            "CREATE TABLE tracks(id INTEGER PRIMARY KEY, title TEXT, visible INTEGER, duration INTEGER,"
            "visual_artist_id INTEGER, album_id INTEGER, visual_genre_id INTEGER, disc INTEGER, track INTEGER);"
            "INSERT INTO tracks VALUES(1,'',1,2,1,1,1,1,1),(2,'',1,1,1,1,1,1,2),"
            "(3,'',1,3,1,2,1,1,1),(4,'Hidden',0,4,1,2,1,1,2);"
            "CREATE TABLE artists(id INTEGER PRIMARY KEY,name TEXT); INSERT INTO artists VALUES(1,'Artist');"
            "CREATE TABLE albums(id INTEGER PRIMARY KEY,name TEXT); INSERT INTO albums VALUES(1,'A'),(2,'B');"
            "CREATE TABLE genres(id INTEGER PRIMARY KEY,name TEXT); INSERT INTO genres VALUES(1,'Genre');"
            "CREATE TABLE meta_keys(id INTEGER PRIMARY KEY,name TEXT); INSERT INTO meta_keys VALUES(1,'empty'),(2,'multi');"
            "CREATE TABLE meta_values(id INTEGER PRIMARY KEY,meta_key_id INTEGER,sort_order INTEGER,content TEXT);"
            "INSERT INTO meta_values VALUES(1,1,0,''),(2,2,0,'First'),(3,2,1,'Second');"
            "CREATE TABLE track_meta(track_id INTEGER,meta_value_id INTEGER); INSERT INTO track_meta VALUES(1,1),(1,2),(1,3);",
            nullptr, nullptr, nullptr) != SQLITE_OK) return false;
        /* Bound UTF-8 and 64 KiB text detect truncation and byte/character length
           confusion without embedding large values in SQL source text. */
        sqlite3_stmt* insert = nullptr;
        if (sqlite3_prepare_v2(raw, "UPDATE tracks SET title=? WHERE id=?", -1, &insert, nullptr) != SQLITE_OK) return false;
        const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(insert, sqlite3_finalize);
        for (int id = 1; id <= 2; ++id) {
            const auto& title = id == 1 ? unicode : longTitle;
            if (sqlite3_bind_text64(insert, 1, title.data(), title.size(), SQLITE_STATIC, SQLITE_UTF8) != SQLITE_OK ||
                sqlite3_bind_int(insert, 2, id) != SQLITE_OK || sqlite3_step(insert) != SQLITE_DONE ||
                sqlite3_reset(insert) != SQLITE_OK) return false;
        }

        std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(test_host_event_create(), test_host_event_destroy);
        if (!event) return false;
        api::TrackResult search, ordered;
        api::TrackPage page;
        api::TrackRef reference;
        unsigned calls = 0;
        mc_result status = MC_ERROR_INTERNAL;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        options.track_query_default_limit = 2;
        options.track_query_max_limit = 3;
        api::Context context(options);
        api::Library library(root);
        /* Keep callback count/status beside the owned payload so a successful
           wait cannot hide duplicate delivery or a stale page from an error. */
        const auto receive = [&](mc_result result, api::TrackPage value, std::string_view) {
            ++calls;
            status = result;
            page = std::move(value);
        };

        /* A wakeup is only a scheduling signal; dispatch and assert this
           operation's expected callback count before reading its page. */
        const auto dispatch = [&](unsigned expected) {
            return test_host_event_wait(event.get()) && context.Dispatch() == MC_OK && calls == expected;
        };
        /* Missing and explicit-empty metadata must remain distinguishable so
           bindings can represent absence without guessing from string length. */
        api::QueryTracks(context, library, {"", {MC_TRACK_FIELD_TITLE, "empty", "multi", "missing"}}, receive);
        if (!dispatch(1) || status != MC_OK || page.Size() != 2 || page.Track(0).Title() != unicode ||
            page.Track(1).Title() != longTitle || page.Value(0, 1).data() == nullptr || !page.Value(0, 1).empty() ||
            page.ValueCount(0, 2) != 2 || page.Value(0, 2, 1) != "Second" || page.Value(0, 3).data() ||
            page.Value(SIZE_MAX, 0).data() || page.Value(0, SIZE_MAX).data()) return false;
        /* Retain a view now and use it after replacing all page owners below;
           copying the strings here would conceal a broken view lifetime. */
        auto retained = page.Track(0);

        /* Album boundaries/durations belong to the result snapshot, rather
           than being recomputed from each later page of current metadata. */
        api::OpenTrackSearch(context, library, {}, [&](mc_result result, api::TrackResult value, std::string_view) {
            ++calls;
            status = result;
            search = std::move(value);
        });
        if (!dispatch(2) || status != MC_OK || search.Size() != 3 || search.Id(0) != 1 || search.Id(1) != 2 ||
            search.Id(2) != 3 || !search.StartsAlbum(0) || search.StartsAlbum(1) || !search.StartsAlbum(2) ||
            search.AlbumDurationSeconds(0) != 3 || search.AlbumDurationSeconds(2) != 3) return false;
        /* Reordered duplicates and a nonexistent positive ID detect SQL order
           leaking through the public result's saved membership. */
        api::TrackResult byDuration;
        api::OpenTrackSearch(context, library, { "", MC_TRACK_SEARCH_SUBSTRING, MC_TRACK_SEARCH_DURATION },
            [&](mc_result result, api::TrackResult value, std::string_view) {
                ++calls;
                status = result;
                byDuration = std::move(value);
            });
        if (!dispatch(3) || status != MC_OK || byDuration.Size() != 3 ||
            byDuration.Id(0) != 2 || byDuration.Id(1) != 1 || byDuration.Id(2) != 3)
        {
            return false;
        }

        reference = search.Track(0);
        const int64_t ids[] = {2, 1, 2, 999, 3};
        ordered = api::MakeTrackResult(library, ids, 5);
        for (const auto offset : {size_t{0}, size_t{3}, size_t{5}, SIZE_MAX}) {
            const auto expected = calls + 1;
            api::ReadTracks(context, ordered, {offset, 3, {MC_TRACK_FIELD_TITLE}}, receive);
            if (!dispatch(expected) || status != MC_OK) return false;
            /* Maximum-sized pages must preserve duplicates, not deduplicate
               to match the SQL IN result that implements the read. */
            if (offset == 0 && (page.Size() != 3 || page.Id(0) != 2 || page.Id(1) != 1 || page.Id(2) != 2 ||
                page.Track(0).Title() != longTitle || page.Track(1).Title() != unicode)) return false;
            /* A missing row keeps its position; the following empty title
               remains a real value rather than being treated as missing. */
            if (offset == 3 && (page.Size() != 2 || page.Id(0) != 999 || page.Value(0, 0).data() ||
                page.Id(1) != 3 || !page.Value(1, 0).empty() || !page.Value(1, 0).data())) return false;
            /* Out-of-range offsets produce an empty page without wrapping. */
            if (offset >= 5 && page.Size()) return false;
        }

        /* Empty reads still describe their projection, allowing callers to
           inspect a successful page uniformly without a special null result. */
        const auto empty = api::MakeTrackResult(library, nullptr, 0);
        auto expected = calls + 1;
        api::ReadTracks(context, empty, {0, 0, {MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ALBUM}}, receive);
        if (!dispatch(expected) || status != MC_OK || page.Size() || page.FieldCount() != 2) return false;
        /* Release inputs before dispatch: accepted work must retain everything
           it needs, and a zero limit must use the context default of two. */
        expected = calls + 1;
        api::ReadTracks(context, ordered, {}, receive);
        ordered = {};
        library = api::Library(static_cast<mc_library*>(nullptr));
        if (!dispatch(expected) || status != MC_OK || page.Size() != 2 || page.Id(0) != 2) return false;

        /* Fail only after the C page is ready, targeting the wrapper's shared
           owner allocation. It must release the page and report one failure. */
        expected = calls + 1;
        api::ReadTrack(context, reference, receive);
        if (!test_host_event_wait(event.get())) return false;
        TestFailNextAllocation();
        if (context.Dispatch() != MC_OK || calls != expected || status != MC_ERROR_INTERNAL || page) return false;

        /* Hide a track after taking the snapshot: direct lookup becomes absent,
           while range reads keep the old slot and album summary unchanged. */
        if (sqlite3_exec(raw, "UPDATE tracks SET visible=0 WHERE id=1", nullptr, nullptr, nullptr) != SQLITE_OK) return false;
        expected = calls + 1;
        api::ReadTrack(context, reference, receive);
        if (!dispatch(expected) || status != MC_ERROR_NOT_FOUND || page) return false;
        expected = calls + 1;
        api::ReadTracks(context, search, {0, 3, {MC_TRACK_FIELD_TITLE}}, receive);
        if (!dispatch(expected) || status != MC_OK || page.Size() != 3 || page.Id(0) != 1 ||
            page.Value(0, 0).data() || search.AlbumDurationSeconds(0) != 3) return false;
        /* Corrupt backend data must report IO instead of publishing zero,
           which callers reserve as the missing/invalid identity sentinel. */
        if (sqlite3_exec(raw, "INSERT INTO tracks VALUES(0,'Invalid ID',1,1,1,1,1,1,0)", nullptr, nullptr, nullptr) != SQLITE_OK) return false;
        api::Library reopened(root);
        expected = calls + 1;
        api::QueryTracks(context, reopened, {}, receive);
        if (!dispatch(expected) || status != MC_ERROR_IO || page) return false;
        /* Successful shutdown and removal of the final explicit page owner
           must not invalidate the retained view's immutable metadata. */
        if (context.Shutdown() != MC_OK) return false;
        page = {};
        return retained.Title() == unicode && retained.Value("multi", 1) == "Second";
    }
}
/* Run the result-lifetime contract against an isolated database and report unexpected exceptions. */
int main(int argc, char** argv) {
    try { if (argc == 2 && check(argv[1])) return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "Result contract exception: %s\n", error.what()); }
    std::fputs("Stable result, page, and wrapper lifetime contract failed\n", stderr);
    return 1;
}
