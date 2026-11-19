#include <musikcore/musikcore.hpp>

#include <cstdio>
#include <thread>
#include <type_traits>
#include <utility>

extern "C" void MC_CALL query_test_wakeup(void *);
extern "C" int query_test_wait_for_wakeup(unsigned count);
extern "C" unsigned query_test_wakeup_count();
extern "C" void query_test_release_wakeup(void);

namespace {
    using musik::core::client::Context;
    using musik::core::client::TrackPage;

    static_assert(!std::is_copy_constructible_v<Context> && !std::is_copy_assignable_v<Context>);
    static_assert(!std::is_move_constructible_v<Context> && !std::is_move_assignable_v<Context>);
    static_assert(!std::is_copy_constructible_v<TrackPage> && !std::is_copy_assignable_v<TrackPage>);
    static_assert(std::is_nothrow_move_constructible_v<TrackPage> && std::is_nothrow_move_assignable_v<TrackPage>);
    static_assert(std::is_same_v<decltype(std::declval<const Context &>().get()), const mc_context *>);
    static_assert(!std::is_invocable_v<decltype(&Context::dispatch), const Context &>);

    bool check_query(const char *directory) {
        using musik::core::client::Error;
        using musik::core::client::query_tracks;
        using musik::core::client::TrackQuery;

        TrackPage page;
        unsigned completed = 0;
        unsigned cancelled = 0;
        bool on_dispatcher = false;
        bool cancelled_without_page = false;
        const auto dispatcher = std::this_thread::get_id();

        {
            mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
            options.data_directory = { directory, std::char_traits<char>::length(directory) };
            options.wakeup = query_test_wakeup;
            Context context(options);
            struct ReleaseWakeup {
                ~ReleaseWakeup() { query_test_release_wakeup(); }
            } release;

            try {
                Context second;
                return false;
            } catch (const Error &error) {
                if (error.result() != MC_ERROR_BUSY) return false;
            }

            /* Hold the worker after its first query so the second is cancelled while queued. */
            const TrackQuery query{ "Morning Light", { MC_TRACK_FIELD_TITLE, "composer" } };
            const auto first = query_tracks(context, query,
                [&](mc_result result, TrackPage received, std::string_view) {
                    ++completed;
                    on_dispatcher = std::this_thread::get_id() == dispatcher;
                    if (result == MC_OK) page = std::move(received);
                });
            if (!first || !query_test_wait_for_wakeup(1)) return false;

            const auto second = query_tracks(context, query,
                [&](mc_result result, TrackPage received, std::string_view) {
                    ++cancelled;
                    cancelled_without_page = result == MC_ERROR_CANCELLED && !received;
                });
            if (!second || first == second || context.cancel(second) != MC_OK) return false;
            query_test_release_wakeup();
            if (context.dispatch() != MC_OK || context.shutdown() != MC_OK) return false;
        }

        /* The wrapper owns its result beyond context shutdown. */
        const auto track = page.track(0);
        const auto composers = track.values("composer");
        return completed == 1 && cancelled == 1 && on_dispatcher && cancelled_without_page &&
            page && page.size() == 1 && page.field_name(0) == MC_TRACK_FIELD_TITLE &&
            track.title() == "Morning Light" && composers.size() == 2 &&
            track.value("composer", 1) == composers[1] && track.artist().data() == nullptr &&
            !track.duration() && !page.track(1);
    }

    bool check_search(const char *directory) {
        using musik::core::client::Error;
        using musik::core::client::query_tracks;
        using musik::core::client::TrackQuery;

        TrackPage visible;
        unsigned generation = 0;
        unsigned delivered = 0;
        unsigned stale = 0;
        mc_result result = MC_ERROR_INTERNAL;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.data_directory = { directory, std::char_traits<char>::length(directory) };
        options.wakeup = query_test_wakeup;
        options.track_query_max_limit = 1;
        Context context(options);

        const auto search = [&](std::string_view text, size_t offset = 0) {
            const auto current = ++generation;
            return query_tracks(context, TrackQuery{ text, { MC_TRACK_FIELD_TITLE }, offset, 1 },
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
            query_tracks(context, TrackQuery{ "", {}, 0, 2 },
                [marker](mc_result, TrackPage, std::string_view) {});
            return false;
        } catch (const Error &error) {
            if (error.result() != MC_ERROR_INVALID_ARGUMENT || marker.use_count() != 1) return false;
        }

        /* Switch searches after the old result is ready, when cancellation is too late. */
        auto wakes = query_test_wakeup_count();
        const auto old = search("Morning");
        if (!query_test_wait_for_wakeup(++wakes) || context.cancel(old) != MC_ERROR_NOT_FOUND) return false;
        search("Evening");
        if (context.dispatch() != MC_OK || stale != 1) return false;
        if (!delivered && (!query_test_wait_for_wakeup(++wakes) || context.dispatch() != MC_OK)) return false;
        if (delivered != 1 || result != MC_OK || visible.size() != 1 ||
            visible.value(0, 0) != "Evening Flight") return false;

        /* Scroll the fixture one row at a time and check the end-of-list page. */
        wakes = query_test_wakeup_count();
        search("", 0);
        if (!query_test_wait_for_wakeup(++wakes) || context.dispatch() != MC_OK || visible.size() != 1) return false;
        TrackPage first = std::move(visible);
        if (visible) return false;

        search("", 1);
        if (!query_test_wait_for_wakeup(++wakes) || context.dispatch() != MC_OK ||
            visible.size() != 1 || first.id(0) == visible.id(0)) return false;
        search("", 2);
        return query_test_wait_for_wakeup(++wakes) && context.dispatch() == MC_OK &&
            delivered == 4 && stale == 1 && result == MC_OK && visible && visible.size() == 0;
    }

    bool check_result_windows(const char *directory) {
        using namespace musik::core::client;
        static_assert(!std::is_copy_constructible_v<TrackResult>);
        static_assert(std::is_nothrow_move_constructible_v<TrackResult>);

        TrackResult tracks;
        TrackPage page;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.data_directory = { directory, std::char_traits<char>::length(directory) };
        options.wakeup = query_test_wakeup;
        Context context(options);
        auto wakes = query_test_wakeup_count();
        const auto dispatch = [&] {
            return query_test_wait_for_wakeup(++wakes) && context.dispatch() == MC_OK;
        };
        const auto receive_tracks = [&](mc_result status, TrackResult result, std::string_view) {
            if (status == MC_OK) tracks = std::move(result);
        };
        const auto receive_page = [&](mc_result status, TrackPage result, std::string_view) {
            if (status == MC_OK) page = std::move(result);
        };

        /* Artist matching reaches records whose titles do not contain the term. */
        open_track_search(context, TrackSearch{ "  fixture artist  " }, receive_tracks);
        if (!dispatch() || tracks.size() != 2 || !tracks.starts_album(0) || tracks.starts_album(1)) return false;

        const auto second = tracks.id(1);
        auto identity = tracks.track(1);
        read_tracks(context, tracks, TrackRange{ 1, 1, { MC_TRACK_FIELD_TITLE } }, receive_page);
        tracks = {};
        if (!dispatch() || page.size() != 1 || page.id(0) != second || page.value(0, 0) != "Evening Flight") return false;
        if (identity.id() != second) return false;
        read_track(context, identity, receive_page);
        identity = {};
        if (!dispatch() || page.size() != 1) return false;
        const auto row = page.track(0);
        if (row.id() != second || row.title() != "Evening Flight" || row.artist() != "Fixture Artist" ||
            row.album() != "Fixture Album" || row.duration() != std::chrono::seconds(1)) return false;

        /* Title sort reverses the fixture's track-number order and drops grouping. */
        open_track_search(context, TrackSearch{ "Light|Flight", MC_TRACK_SEARCH_REGEX, MC_TRACK_SEARCH_TITLE },
            receive_tracks);
        if (!dispatch() || tracks.size() != 2 || tracks.id(0) != second || tracks.starts_album(0)) return false;

        read_tracks(context, tracks, TrackRange{ 0, 2, { MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_TITLE } }, receive_page);
        if (!dispatch() || page.size() != 2 || page.track(0).title() != "Evening Flight" ||
            page.track(0).track_number() != 2 || page.track(1).title() != "Morning Light") return false;

        read_tracks(context, tracks, TrackRange{ 2, 1, {} }, receive_page);
        return dispatch() && page && page.size() == 0;
    }
}

int main(int argc, char **argv) {
    try {
        if (argc == 2 && check_query(argv[1]) && check_search(argv[1]) && check_result_windows(argv[1])) return 0;
        std::fputs("C++ wrapper query contract failed\n", stderr);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "C++ wrapper query failed: %s\n", error.what());
    }
    return 1;
}
