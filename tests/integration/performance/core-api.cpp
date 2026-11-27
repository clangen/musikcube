#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include "profile.hpp"
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

    /* Measure one completed host operation in microseconds using a monotonic clock. */
    double elapsed(Clock::time_point start) {
        return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    }

    /* Report median and nearest-rank p95 for the collected latency samples. */
    void report(const char* operation, std::vector<double> values) {
        std::sort(values.begin(), values.end());
        std::printf("%s: median_us=%.3f p95_us=%.3f samples=%zu\n", operation,
            values[values.size() / 2], values[(values.size() * 95 + 99) / 100 - 1], values.size());
    }

    /* Build a synthetic 10,000-track catalog with fixed-width titles for reproducible API measurements. */
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
        /* Sample live malloc blocks/bytes on macOS; other platforms leave the observation unavailable as
           zeros. */
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

/* Measure API latency, wrapper allocation/copy overhead, and bounded cache growth in an isolated catalog. */
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        prepare(argv[1]);
        std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
            event(test_host_event_create(), test_host_event_destroy);
        if (!event) return 1;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();

        /* Reserve sample storage before timing so vector growth is not
           mistaken for API latency or profiled core allocations. */
        std::vector<double> creates, shutdowns, searches, reads, results;
        creates.reserve(samples); shutdowns.reserve(samples); searches.reserve(samples);
        reads.reserve(samples); results.reserve(samples);
        /* Context creation is intentionally lazy: this measures lifecycle
           overhead without opening a library or starting playback workers. */
        for (size_t i = 0; i < samples; ++i) {
            ProfilePhase create(i + 1 == samples ? "context_create" : nullptr);
            auto start = Clock::now();
            api::Context context(options);
            creates.push_back(elapsed(start));
            create.Finish();
            ProfilePhase shutdown(i + 1 == samples ? "idle_shutdown" : nullptr);
            start = Clock::now();
            if (context.Shutdown() != MC_OK) return 1;
            shutdowns.push_back(elapsed(start));
            shutdown.Finish();
        }
        /* Measure real query work separately from idle lifecycle overhead. */
        api::TrackResult result;
        api::TrackPage page;
        mc_result completed = MC_ERROR_INTERNAL;
        api::Context context(options);
        api::Library library(argv[1]);
        /* Count latency through terminal callback delivery, not merely worker
           admission; a fast submission with stalled dispatch is not success. */
        const auto drain = [&] {
            if (!test_host_event_wait(event.get()) || context.Dispatch() != MC_OK || completed != MC_OK) {
                throw api::Error(completed);
            }
        };
        /* Keep membership large but read only a bounded page, matching list
           consumers that must avoid eagerly loading every metadata string. */
        for (size_t i = 0; i < samples; ++i) {
            ProfilePhase searchProfile(i + 1 == samples ? "search_10000_ids" : nullptr);
            auto start = Clock::now();
            api::OpenTrackSearch(context, library, { {}, MC_TRACK_SEARCH_TITLE },
                [&](mc_result status, api::TrackResult value, std::string_view) {
                    completed = status;
                    result = std::move(value);
                });
            drain();
            if (result.Size() != rows) return 1;
            searches.push_back(elapsed(start));
            searchProfile.Finish();
            ProfilePhase readProfile(i + 1 == samples ? "read_1000_titles" : nullptr);
            start = Clock::now();
            api::ReadTracks(context, result, {0, 1000, {MC_TRACK_FIELD_TITLE}},
                [&](mc_result status, api::TrackPage value, std::string_view) {
                    completed = status; page = std::move(value);
                });
            drain();
            if (page.Size() != 1000) return 1;
            reads.push_back(elapsed(start));
            readProfile.Finish();
        }
        /* Compare equivalent C/C++ submissions on the caller thread. Worker
           allocations cannot be attributed to wrapper callback state. */
        const mc_string_view field{MC_TRACK_FIELD_TITLE, sizeof(MC_TRACK_FIELD_TITLE) - 1};
        mc_track_range rawRange = MC_TRACK_RANGE_DEFAULT;
        rawRange.fields = &field; rawRange.field_count = 1; rawRange.limit = 1000;
        struct RawCompletion { mc_result result; mc_track_page* page; } raw{MC_ERROR_INTERNAL, nullptr};
        ProfilePhase rawSubmit("submit_page_c", true);
        mc_request_id request = 0;
        const auto submitted = mc_track_result_read_async(context.Get(), result.Get(), &rawRange,
            [](mc_result status, mc_track_page* value, const mc_error*, void* data) {
                auto& output = *static_cast<RawCompletion*>(data); output.result = status; output.page = value;
            }, &raw, &request);
        const auto cCounts = rawSubmit.Finish();
        if (submitted != MC_OK || !test_host_event_wait(event.get()) || context.Dispatch() != MC_OK || raw.result != MC_OK) return 1;

        /* Isolate page adoption from the C read so any wrapper owner allocation
           is visible without charging metadata creation to the wrapper. */
        ProfilePhase adoption("adopt_page_cpp", true);
        auto adopted = api::TrackPage(std::exchange(raw.page, nullptr));
        const auto adoptionCounts = adoption.Finish();
        if (adoptionCounts.allocations > 1 || adoptionCounts.copied_bytes) return 1;
        adopted = {};
        const api::TrackRange range{0, 1000, {MC_TRACK_FIELD_TITLE}};
        ProfilePhase cppSubmit("submit_page_cpp", true);
        api::ReadTracks(context, result, range, [&](mc_result status, api::TrackPage value, std::string_view) {
            completed = status; page = std::move(value);
        });
        const auto cppCounts = cppSubmit.Finish();
        drain();
        if (cppCounts.allocations > cCounts.allocations + 4 ||
            cppCounts.requested_bytes > cCounts.requested_bytes + 64 ||
            cppCounts.copied_bytes > cCounts.copied_bytes) return 1;
        /* Duplicate IDs are deliberate: result construction must copy the
           caller's sequence rather than normalize it into unique membership. */
        const std::vector<int64_t> repeated(rows, result.Id(0));
        for (size_t i = 0; i < samples; ++i) {
            ProfilePhase profile(i + 1 == samples ? "make_result_10000_ids" : nullptr);
            const auto start = Clock::now();
            const auto copy = api::MakeTrackResult(library, repeated.data(), repeated.size());
            if (copy.Size() != rows || copy.Id(rows - 1) != repeated.front()) return 1;
            results.push_back(elapsed(start));
            profile.Finish();
        }
        /* Reading page-owned strings through wrapper views must not allocate or copy bytes. */
        ProfilePhase borrowed("borrowed_titles_1000");
        const AllocationSnapshot before;
        size_t bytes = 0;
        for (size_t i = 0; i < page.Size(); ++i) {
            const auto title = page.Track(i).Title();
            if (title.data() != page.Value(i, 0).data()) return 1;
            bytes += title.size();
        }
        const AllocationSnapshot after;
        const auto borrowedCounts = borrowed.Finish();
        if (borrowedCounts.allocations || borrowedCounts.copied_bytes) return 1;
        if (before.blocks != after.blocks || before.bytes != after.bytes) return 1;
        report("context_create", creates);
        report("idle_shutdown", shutdowns);
        report("search_10000_ids", searches);
        report("read_1000_titles", reads);
        report("make_result_10000_ids", results);
        std::printf("borrowed_title_access: rows=%zu payload_bytes=%zu copied_bytes=0", page.Size(), bytes);
#ifdef __APPLE__
        std::printf(" live_blocks_delta=%lld live_bytes_delta=%lld", static_cast<long long>(after.blocks) - before.blocks,
            static_cast<long long>(after.bytes) - before.bytes);
#endif
        std::puts("");
        /* Walk the entire catalog with a three-page cache. Measuring after
           warmup distinguishes bounded reuse from growth on each new viewport. */
        {
            api::TrackResultWindow window(context, std::move(result), {MC_TRACK_FIELD_TITLE},
                [&](size_t, size_t, mc_result status) { completed = status; });
            const AllocationSnapshot emptyCache;
            size_t maximumBytes = emptyCache.bytes;
            size_t warmedBytes = 0;
            ProfilePhase profile("scroll_100_pages_capacity_3");
            const auto start = Clock::now();
            for (size_t first = 0; first < rows; first += 100) {
                window.SetViewport(first, first + 99, 100);
                if (window.Load() != MC_OK) return 1;
                while (window.Pending()) drain();
                const auto visible = window.CachedRow(first);
                if (!visible || visible->Title().size() != 128) return 1;
                const AllocationSnapshot cache;
                maximumBytes = std::max(maximumBytes, cache.bytes);
                if (first == 1900) warmedBytes = cache.bytes;
            }
            const auto time = elapsed(start);
            profile.Finish();
            const AllocationSnapshot finalCache;
#ifdef __APPLE__
            /* Includes malloc rounding and asynchronous request bookkeeping.
               The allowance is fixed, independent of total result row count. */
            if (maximumBytes > emptyCache.bytes + 512 * 1024 || finalCache.bytes > warmedBytes + 64 * 1024) return 1;
#endif
            std::printf("scroll_100_pages: total_us=%.3f cache_peak_extra_bytes=%lld cache_growth_after_warm_bytes=%lld\n",
                time, static_cast<long long>(maximumBytes) - emptyCache.bytes,
                std::max<long long>(0, static_cast<long long>(finalCache.bytes) - warmedBytes));
        }
        if (context.Shutdown() != MC_OK) return 1;
        return 0;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Baseline failed: %s\n", error.what());
        return 1;
    }
}
