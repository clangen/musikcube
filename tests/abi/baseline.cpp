#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif

namespace {
    using Clock = std::chrono::steady_clock;
    namespace api = musik::core::api;
    constexpr size_t rows = 10000;
    constexpr size_t samples = 31;

    double elapsed(Clock::time_point start) {
        return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    }

    void report(const char* operation, std::vector<double> values) {
        std::sort(values.begin(), values.end());
        std::printf("%s: median_us=%.3f p95_us=%.3f samples=%zu\n", operation,
            values[values.size() / 2], values[(values.size() - 1) * 95 / 100], values.size());
    }

    void prepare(const char* root) {
        sqlite3* database = nullptr;
        const auto path = std::string(root) + "/musik.db";
        if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) throw api::Error(MC_ERROR_IO);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> owned(database, sqlite3_close);
        const auto status = sqlite3_exec(database,
            "CREATE TABLE tracks(id INTEGER PRIMARY KEY, title TEXT, visible INTEGER, duration INTEGER,"
            "visual_artist_id INTEGER, album_id INTEGER, visual_genre_id INTEGER, disc INTEGER, track INTEGER);"
            "WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<10000)"
            "INSERT INTO tracks SELECT x, printf('%0128d', x), 1, 120, 1, 1, 1, 1, x FROM n;"
            "CREATE TABLE artists(id INTEGER PRIMARY KEY, name TEXT); INSERT INTO artists VALUES(1, 'Artist');"
            "CREATE TABLE albums(id INTEGER PRIMARY KEY, name TEXT); INSERT INTO albums VALUES(1, 'Album');"
            "CREATE TABLE genres(id INTEGER PRIMARY KEY, name TEXT); INSERT INTO genres VALUES(1, 'Genre');"
            "CREATE TABLE meta_keys(id INTEGER PRIMARY KEY, name TEXT);"
            "CREATE TABLE meta_values(id INTEGER PRIMARY KEY, meta_key_id INTEGER, sort_order INTEGER, content TEXT);"
            "CREATE TABLE track_meta(track_id INTEGER, meta_value_id INTEGER);", nullptr, nullptr, nullptr);
        if (status != SQLITE_OK) throw api::Error(MC_ERROR_IO, sqlite3_errmsg(database));
    }

    struct AllocationSnapshot {
        size_t blocks = 0;
        size_t bytes = 0;
        AllocationSnapshot() {
#ifdef __APPLE__
            malloc_statistics_t statistics{};
            malloc_zone_statistics(nullptr, &statistics);
            blocks = statistics.blocks_in_use;
            bytes = statistics.size_in_use;
#endif
        }
    };
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        prepare(argv[1]);
        std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
            event(test_host_event_create(), test_host_event_destroy);
        if (!event) return 1;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        std::vector<double> creates, shutdowns, searches, reads;
        for (size_t i = 0; i < samples; ++i) {
            auto start = Clock::now();
            api::Context context(options);
            creates.push_back(elapsed(start));
            start = Clock::now();
            if (context.Shutdown() != MC_OK) return 1;
            shutdowns.push_back(elapsed(start));
        }
        api::TrackResult result;
        api::TrackPage page;
        mc_result completed = MC_ERROR_INTERNAL;
        api::Context context(options);
        api::Library library(argv[1]);
        const auto drain = [&] {
            if (!test_host_event_wait(event.get()) || context.Dispatch() != MC_OK || completed != MC_OK) {
                throw api::Error(completed);
            }
        };
        for (size_t i = 0; i < samples; ++i) {
            auto start = Clock::now();
            api::OpenTrackSearch(context, library, {"", MC_TRACK_SEARCH_SUBSTRING, MC_TRACK_SEARCH_TITLE},
                [&](mc_result status, api::TrackResult value, std::string_view) {
                    completed = status; result = std::move(value);
                });
            drain();
            if (result.Size() != rows) return 1;
            searches.push_back(elapsed(start));
            start = Clock::now();
            api::ReadTracks(context, result, {0, 1000, {MC_TRACK_FIELD_TITLE}},
                [&](mc_result status, api::TrackPage value, std::string_view) {
                    completed = status; page = std::move(value);
                });
            drain();
            if (page.Size() != 1000) return 1;
            reads.push_back(elapsed(start));
        }
        /* Accessing borrowed wrapper values must neither allocate nor copy strings. */
        const AllocationSnapshot before;
        size_t bytes = 0;
        for (size_t i = 0; i < page.Size(); ++i) {
            const auto title = page.Track(i).Title();
            if (title.data() != page.Value(i, 0).data()) return 1;
            bytes += title.size();
        }
        const AllocationSnapshot after;
        if (before.blocks != after.blocks || before.bytes != after.bytes) return 1;
        report("context_create", creates);
        report("idle_shutdown", shutdowns);
        report("search_10000_ids", searches);
        report("read_1000_titles", reads);
        std::printf("borrowed_title_access: rows=%zu payload_bytes=%zu copied_bytes=0", page.Size(), bytes);
#ifdef __APPLE__
        std::printf(" live_blocks_delta=%lld live_bytes_delta=%lld", static_cast<long long>(after.blocks) - before.blocks,
            static_cast<long long>(after.bytes) - before.bytes);
#endif
        std::puts("");
        if (context.Shutdown() != MC_OK) return 1;
        return 0;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Baseline failed: %s\n", error.what());
        return 1;
    }
}
