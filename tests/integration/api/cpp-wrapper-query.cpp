#include "cpp-wrapper-cases.h"
#include <musikcore/musikcore.hpp>

#ifdef MUSIKCORE_TEST_PRODUCTION_RUNTIME
#include "../../support/CoreRuntime.h"
#endif

#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>

namespace wrapper_tests {
    using musik::core::api::Context;
    using musik::core::api::Library;
    using musik::core::api::TrackPage;

    static_assert(!std::is_copy_constructible_v<Context> && !std::is_copy_assignable_v<Context>);
    static_assert(!std::is_move_constructible_v<Context> && !std::is_move_assignable_v<Context>);
    static_assert(!std::is_copy_constructible_v<TrackPage> && !std::is_copy_assignable_v<TrackPage>);
    static_assert(std::is_nothrow_move_constructible_v<TrackPage> && std::is_nothrow_move_assignable_v<TrackPage>);
    static_assert(std::is_same_v<decltype(std::declval<const Context &>().Get()), const mc_context *>);
    static_assert(!std::is_invocable_v<decltype(&Context::Dispatch), const Context &>);

    /* Open the same fixture through either the public standalone identity or the production library adapter. */
    static std::shared_ptr<const Library> openLibrary(const char *directory) {
#ifdef MUSIKCORE_TEST_PRODUCTION_RUNTIME
        return OpenFixtureLibrary();
#else
        return std::make_shared<const Library>(directory);
#endif
    }

    /* Retain views while replacing their pages to catch use-after-free without copying metadata strings. */
    bool checkQuery(const char *directory) {
        using musik::core::api::Error;
        using musik::core::api::QueryTracks;
        using musik::core::api::TrackQuery;

        TrackPage page;
        unsigned completed = 0;
        unsigned cancelled = 0;
        bool on_dispatcher = false;
        bool cancelled_without_page = false;
        const auto dispatcher = std::this_thread::get_id();

        {
            mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
            options.services = MC_SERVICE_NONE;
            auto library = openLibrary(directory);
            if (!library) return false;
            options.wakeup = query_test_wakeup;
            Context context(options);
            struct ReleaseWakeup {
                ~ReleaseWakeup() { query_test_release_wakeup(); }
            } release;

            try {
                Context second;
                return false;
            } catch (const Error &error) {
                if (error.Result() != MC_ERROR_BUSY) return false;
            }

            /* Hold the worker after its first query so the second is cancelled while queued. */
            const TrackQuery query{ "Morning Light", { MC_TRACK_FIELD_TITLE, "composer" } };
            const auto first = QueryTracks(context, *library, query,
                [&](mc_result result, TrackPage received, std::string_view) {
                    ++completed;
                    on_dispatcher = std::this_thread::get_id() == dispatcher;
                    if (result == MC_OK) page = std::move(received);
                });
            if (!first || !query_test_wait_for_wakeup(1)) return false;

            const auto second = QueryTracks(context, *library, query,
                [&](mc_result result, TrackPage received, std::string_view) {
                    ++cancelled;
                    cancelled_without_page = result == MC_ERROR_CANCELLED && !received;
                });
            if (!second || first == second || context.Cancel(second) != MC_OK) return false;
            query_test_release_wakeup();
            if (context.Dispatch() != MC_OK || context.Shutdown() != MC_OK) return false;
        }

        /* The wrapper owns its result beyond context shutdown. */
        const auto track = page.Track(0);
        const auto composers = track.Values("composer");
        const bool valuesMatch = completed == 1 && cancelled == 1 && on_dispatcher && cancelled_without_page &&
            page && page.Size() == 1 && page.FieldName(0) == MC_TRACK_FIELD_TITLE &&
            track.Title() == "Morning Light" && composers.size() == 2 &&
            track.Value("composer", 1) == composers[1] && track.Artist().data() == nullptr &&
            !track.Duration() && !page.Track(1);
        auto moved = std::move(page);
        if (page || !moved) return false;
        moved = {};
        auto copiedView = track;
        auto movedView = std::move(copiedView);
        return valuesMatch && !copiedView && movedView.Title() == "Morning Light" &&
            movedView.Value("composer", 1) == composers[1];
    }

    /* Verify ordered searches, grouping, projected reads, and track identities through the wrapper. */
    bool checkSearch(const char *directory) {
        using musik::core::api::Error;
        using musik::core::api::QueryTracks;
        using musik::core::api::TrackQuery;

        TrackPage visible;
        unsigned generation = 0;
        unsigned delivered = 0;
        unsigned stale = 0;
        mc_result result = MC_ERROR_INTERNAL;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        auto library = openLibrary(directory);
        if (!library) return false;
        options.wakeup = query_test_wakeup;
        options.track_query_max_limit = 1;
        Context context(options);

        const auto search = [&](std::string_view text, size_t offset = 0) {
            const auto current = ++generation;
            return QueryTracks(context, *library, TrackQuery{ text, { MC_TRACK_FIELD_TITLE }, offset, 1 },
                [&, current](mc_result status, TrackPage page, std::string_view) {
                    if (current != generation) {
                        ++stale;
                        return;
                    }
                    ++delivered;
                    result = status;
                    visible = std::move(page);
                });
        };

        /* A rejected request must release the wrapper's captured callback state. */
        const auto marker = std::make_shared<int>(0);
        try {
            QueryTracks(context, *library, TrackQuery{ "", {}, 0, 2 },
                [marker](mc_result, TrackPage, std::string_view) {});
            return false;
        } catch (const Error &error) {
            if (error.Result() != MC_ERROR_INVALID_ARGUMENT || marker.use_count() != 1) return false;
        }

        /* Switch searches after the old result is ready, when cancellation is too late. */
        auto wakes = query_test_wakeup_count();
        const auto old = search("Morning");
        if (!query_test_wait_for_wakeup(++wakes) || context.Cancel(old) != MC_ERROR_NOT_FOUND) return false;
        search("Evening");
        if (context.Dispatch() != MC_OK || stale != 1) return false;
        if (!delivered && (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK)) return false;
        if (delivered != 1 || result != MC_OK || visible.Size() != 1 ||
            visible.Value(0, 0) != "Evening Flight") return false;

        /* Scroll the fixture one row at a time and check the end-of-list page. */
        wakes = query_test_wakeup_count();
        search("", 0);
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK || visible.Size() != 1) return false;
        TrackPage first = std::move(visible);
        if (visible) return false;

        search("", 1);
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK ||
            visible.Size() != 1 || first.Id(0) == visible.Id(0)) return false;
        search("", 2);
        return query_test_wait_for_wakeup(++wakes) && context.Dispatch() == MC_OK &&
            delivered == 4 && stale == 1 && result == MC_OK && visible && visible.Size() == 0;
    }

    /* Move/invalidate viewports while reads are pending so stale callbacks cannot repopulate the wrong
       window. */
    bool checkResultWindows(const char *directory) {
        using namespace musik::core::api;
        static_assert(!std::is_copy_constructible_v<TrackResult>);
        static_assert(std::is_nothrow_move_constructible_v<TrackResult>);

        TrackResult tracks;
        TrackPage page;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        auto library = openLibrary(directory);
        if (!library) return false;
        options.wakeup = query_test_wakeup;
        options.track_query_max_limit = 2;
        Context context(options);
        auto wakes = query_test_wakeup_count();
        const auto dispatch = [&] {
            return query_test_wait_for_wakeup(++wakes) && context.Dispatch() == MC_OK;
        };
        const auto receive_tracks = [&](mc_result status, TrackResult result, std::string_view) {
            if (status == MC_OK) tracks = std::move(result);
        };
        const auto receive_page = [&](mc_result status, TrackPage result, std::string_view) {
            if (status == MC_OK) page = std::move(result);
        };

        /* Artist matching reaches records whose titles do not contain the term. */
        OpenTrackSearch(context, *library, TrackSearch{ "  fixture artist  " }, receive_tracks);
        if (!dispatch() || tracks.Size() != 2 || !tracks.StartsAlbum(0) || tracks.StartsAlbum(1) ||
            tracks.AlbumDurationSeconds(0) != 2 || tracks.AlbumDurationSeconds(1) != 0) return false;

        const auto second = tracks.Id(1);
        auto identity = tracks.Track(1);
        ReadTracks(context, tracks, TrackRange{ 1, 1, { MC_TRACK_FIELD_TITLE } }, receive_page);
        tracks = {};
        if (!dispatch() || page.Size() != 1 || page.Id(0) != second || page.Value(0, 0) != "Evening Flight") return false;
        if (identity.Id() != second) return false;
        ReadTrack(context, identity, receive_page);
        identity = {};
        if (!dispatch() || page.Size() != 1) return false;
        const auto row = page.Track(0);
        if (row.Id() != second || row.Title() != "Evening Flight" || row.Artist() != "Fixture Artist" ||
            row.Album() != "Fixture Album" || row.Duration() != std::chrono::seconds(1)) return false;

        /* Title sort reverses the fixture's track-number order and drops grouping. */
        OpenTrackSearch(context, *library, TrackSearch{ "Light|Flight", MC_TRACK_SEARCH_REGEX, MC_TRACK_SEARCH_TITLE },
            receive_tracks);
        if (!dispatch() || tracks.Size() != 2 || tracks.Id(0) != second || tracks.StartsAlbum(0)) return false;

        ReadTracks(context, tracks, TrackRange{ 0, 2, { MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_TITLE } }, receive_page);
        if (!dispatch() || page.Size() != 2 || page.Track(0).Title() != "Evening Flight" ||
            page.Track(0).TrackNumber() != 2 || page.Track(1).Title() != "Morning Light") return false;

        ReadTracks(context, tracks, TrackRange{ 2, 1, {} }, receive_page);
        if (!dispatch() || !page || page.Size()) return false;

        using Update = TrackResultWindow::ViewportUpdate;
        TrackResultWindow bounded(context, std::move(tracks), { MC_TRACK_FIELD_TITLE }, {},
            TrackResultWindow::Options{ 1, 0 });
        if (bounded.SetViewport(0, 0, 1) != Update::Changed ||
            bounded.SetViewport(0, 1, 1) != Update::ExceedsCapacity ||
            bounded.SetViewport(0, 0, 0) != Update::InvalidRange ||
            bounded.SetViewport(0, 0, 1) != Update::Unchanged) return false;

        OpenTrackSearch(context, *library, TrackSearch{ "Light|Flight", MC_TRACK_SEARCH_REGEX, MC_TRACK_SEARCH_TITLE },
            receive_tracks);
        if (!dispatch() || tracks.Size() != 2) return false;

        unsigned changes = 0;
        mc_result latest = MC_ERROR_INTERNAL;
        TrackResultWindow window(context, std::move(tracks), { MC_TRACK_FIELD_TITLE },
            [&](size_t, size_t, mc_result status) {
                ++changes;
                latest = status;
            });
        if (window.SetViewport(0, 0, 1) != Update::Changed || window.Load() != MC_OK || !window.Pending() ||
            !dispatch() || !window.Pending() || changes != 1 || latest != MC_OK) return false;
        const auto visible = window.CachedRow(0);
        if (!visible || visible->Title() != "Evening Flight") return false;

        /* Release a page already queued for dispatch after invalidating the window. */
        if (!query_test_wait_for_wakeup(++wakes)) return false;
        window.Clear();
        if (context.Dispatch() != MC_OK || changes != 1 || window.Pending() ||
            window.CachedRow(0) || window.CachedRow(1)) return false;

        if (window.SetViewport(1, 1, 1) != Update::Changed || window.Load() != MC_OK || !dispatch() ||
            !window.CachedRow(1) || window.CachedRow(1)->Title() != "Morning Light") return false;
        if (window.Pending() && !dispatch()) return false;

        /* Rejected submission has no callback and must leave the window idle. */
        const auto before = changes;
        if (window.SetViewport(0, 1, 3) != Update::Changed || window.Load() != MC_ERROR_INVALID_ARGUMENT ||
            window.Pending() || changes != before) return false;

        /* Undispatched requests fill the context even if their worker has finished. */
        if (window.SetViewport(0, 1, 2) != Update::Changed) return false;
        std::vector<mc_request_id> queued;
        try {
            constexpr size_t queueCapacity = 64;
            for (size_t i = 0; i < queueCapacity; ++i) {
                queued.push_back(QueryTracks(context, *library, TrackQuery{ "" },
                    [](mc_result, TrackPage, std::string_view) {}));
            }
        } catch (const Error&) {
            return false;
        }
        if (window.Load() != MC_ERROR_QUEUE_FULL || window.Pending()) return false;
        for (const auto id : queued) context.Cancel(id);
        if (context.Dispatch() != MC_OK || window.Load() != MC_OK || !window.Pending()) return false;

        return query_test_dispatch_until(context.Get(), [&] { return !window.Pending(); }) &&
            changes == before + 1 && latest == MC_OK &&
            window.CachedRow(0) && window.CachedRow(1);
    }
}
