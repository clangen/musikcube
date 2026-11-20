#include <musikcore/musikcore.hpp>

#ifdef MUSIKCORE_TEST_APP_BINDING
#include "../fixtures/TestTransport.h"
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/ffi/consumer/playback-binding.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <limits>
#include <set>
#endif

#include <cstdio>
#include <thread>
#include <type_traits>
#include <utility>

extern "C" void MC_CALL query_test_wakeup(void *);
extern "C" int query_test_wait_for_wakeup(unsigned count);
extern "C" unsigned query_test_wakeup_count();
extern "C" void query_test_release_wakeup(void);

namespace {
    using musik::core::api::Context;
    using musik::core::api::Library;
    using musik::core::api::TrackPage;

    static_assert(!std::is_copy_constructible_v<Context> && !std::is_copy_assignable_v<Context>);
    static_assert(!std::is_move_constructible_v<Context> && !std::is_move_assignable_v<Context>);
    static_assert(!std::is_copy_constructible_v<TrackPage> && !std::is_copy_assignable_v<TrackPage>);
    static_assert(std::is_nothrow_move_constructible_v<TrackPage> && std::is_nothrow_move_assignable_v<TrackPage>);
    static_assert(std::is_same_v<decltype(std::declval<const Context &>().Get()), const mc_context *>);
    static_assert(!std::is_invocable_v<decltype(&Context::Dispatch), const Context &>);

    std::shared_ptr<const Library> openLibrary(const char *directory) {
#ifdef MUSIKCORE_TEST_APP_BINDING
        musik::core::library::MasterLibrary selection;
        return selection.OpenApiLibrary();
#else
        return std::make_shared<const Library>(directory);
#endif
    }

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
        return completed == 1 && cancelled == 1 && on_dispatcher && cancelled_without_page &&
            page && page.Size() == 1 && page.FieldName(0) == MC_TRACK_FIELD_TITLE &&
            track.Title() == "Morning Light" && composers.size() == 2 &&
            track.Value("composer", 1) == composers[1] && track.Artist().data() == nullptr &&
            !track.Duration() && !page.Track(1);
    }

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

    bool checkResultWindows(const char *directory) {
        using namespace musik::core::api;
        static_assert(!std::is_copy_constructible_v<TrackResult>);
        static_assert(std::is_nothrow_move_constructible_v<TrackResult>);

        TrackResult tracks;
        TrackPage page;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
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
        if (window.Load() != MC_ERROR_BUSY || window.Pending()) return false;
        for (const auto id : queued) context.Cancel(id);
        if (context.Dispatch() != MC_OK || window.Load() != MC_OK || !window.Pending()) return false;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (window.Pending() && std::chrono::steady_clock::now() < deadline) {
            if (context.Dispatch() != MC_OK) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return !window.Pending() && changes == before + 1 && latest == MC_OK &&
            window.CachedRow(0) && window.CachedRow(1);
    }
#ifdef MUSIKCORE_TEST_APP_BINDING
    bool checkPlaybackCommands() {
        using namespace musik::core;
        const auto selection = std::make_shared<library::MasterLibrary>();
        const auto source = selection->OpenApiLibrary();
        if (!source) return false;
        api::TrackResult result;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.wakeup = query_test_wakeup;
        api::Context context(options);
        const auto wakes = query_test_wakeup_count();
        api::OpenTrackSearch(context, *source, api::TrackSearch{ "Fixture Artist" },
            [&](mc_result status, api::TrackResult tracks, std::string_view) {
                if (status == MC_OK) result = std::move(tracks);
            });
        if (!query_test_wait_for_wakeup(wakes + 1) || context.Dispatch() != MC_OK || result.Size() != 2) return false;

        runtime::MessageQueue queue;
        const auto transport = std::make_shared<TestTransport>();
        std::optional<api::Playback> playback;
        {
            audio::PlaybackService service(queue, selection, transport);
            playback.emplace(musik::core::audio::CreatePlaybackHandle(service));
            if (playback->Control(MC_PLAYBACK_TOGGLE_PAUSE) != MC_OK ||
                transport->state != sdk::PlaybackState::Paused ||
                playback->Control(MC_PLAYBACK_SET_VOLUME, 0.25) != MC_OK ||
                transport->volume != 0.25 ||
                playback->Control(MC_PLAYBACK_SET_VOLUME, 1.5) != MC_ERROR_INVALID_ARGUMENT ||
                playback->Control(MC_PLAYBACK_STOP) != MC_OK ||
                transport->state != sdk::PlaybackState::Stopped) return false;

            if (playback->Control(MC_PLAYBACK_SET_POSITION, 12) != MC_OK ||
                playback->Control(MC_PLAYBACK_TOGGLE_MUTE) != MC_OK) return false;
            const auto snapshot = playback->Snapshot();
            if (!snapshot || snapshot->state != MC_PLAYBACK_STOPPED || snapshot->volume != 0.25 ||
                snapshot->position != 12 || snapshot->duration != 30 || !snapshot->muted) return false;

            if (playback->Play(result, 2) != MC_ERROR_NOT_FOUND ||
                playback->Play(result, 1) != MC_OK || service.GetIndex() != 1 ||
                service.GetTrackList()->GetId(0) != result.Id(0) ||
                service.GetTrackList()->GetId(1) != result.Id(1)) return false;
            if (playback->Move(1, 0) != MC_OK || service.GetTrackList()->GetId(0) != result.Id(1) ||
                playback->Remove(1) != MC_OK || service.Count() != 1 ||
                playback->Remove(1) != MC_ERROR_NOT_FOUND ||
                playback->ClearQueue() != MC_OK || service.Count() != 0) return false;

            const auto first = result.Track(0);
            const auto second = result.Track(1);
            const auto trackList = service.GetTrackList();
            if (playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NOW) != MC_OK ||
                transport->state != sdk::PlaybackState::Playing ||
                service.Count() != 1 || trackList->GetId(0) != first.Id() ||
                playback->UseTrack(first, MC_PLAYBACK_TRACK_APPEND) != MC_OK ||
                playback->UseTrack(second, MC_PLAYBACK_TRACK_PLAY_NEXT) != MC_OK ||
                service.Count() != 3 ||
                trackList->GetId(1) != second.Id() || trackList->GetId(2) != first.Id() ||
                playback->UseTrack(second, MC_PLAYBACK_TRACK_PREPEND) != MC_OK ||
                service.Count() != 4 || trackList->GetId(0) != second.Id()) return false;

            if (playback->UseTrack({}, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_INVALID_ARGUMENT ||
                playback->UseTrack(first, 0) != MC_ERROR_UNSUPPORTED) return false;

            const auto prefs = Preferences::ForComponent(musik::core::prefs::components::Settings);
            prefs->SetInt(musik::core::prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
            selection->LoadDefaultLibrary();
            const bool rejected = playback->Play(result, 0) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NOW) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NEXT) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_APPEND) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PREPEND) == MC_ERROR_NOT_FOUND &&
                service.Count() == 4 && trackList->GetId(0) == second.Id();
            audio::DiscardUnavailablePlaybackQueue(service);
            const bool cleared = service.Count() == 0 && transport->state == sdk::PlaybackState::Stopped;
            prefs->SetInt(musik::core::prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
            selection->LoadDefaultLibrary();
            if (!rejected || !cleared) return false;

            mc_result otherThread = MC_OK;
            std::optional<api::Playback> otherHandle;
            std::thread caller([&] {
                otherHandle.emplace(musik::core::audio::CreatePlaybackHandle(service));
                otherThread = otherHandle->Control(MC_PLAYBACK_STOP);
            });
            caller.join();
            if (otherThread != MC_ERROR_BUSY || otherHandle->Control(MC_PLAYBACK_STOP) != MC_OK) return false;
        }
        return playback->Control(MC_PLAYBACK_STOP) == MC_ERROR_NOT_FOUND && !playback->Snapshot();
    }

    bool checkRemoteQueryPayloads() {
        using namespace musik::core;
        using namespace musik::core::library;
        using namespace musik::core::library::query;

        MasterLibrary selection;
        const auto local = selection.Wrapped();
        const auto remote = LibraryFactory::Instance().DefaultRemoteLibrary();
        const auto roundTrip = [&](db::ISerializableQuery& remoteQuery) {
            const auto server = QueryRegistry::CreateLocalQueryFor(
                remoteQuery.Name(), remoteQuery.SerializeQuery(), local);
            if (!server || local->EnqueueAndWait(server) < 0 || server->GetStatus() != db::IQuery::Finished) {
                return false;
            }

            remoteQuery.DeserializeResult(server->SerializeResult());
            return remoteQuery.GetStatus() == db::IQuery::Finished;
        };

        /* Exercise the same query registry and payloads the WebSocket path uses. */
        SearchTrackListQuery search(remote, SearchTrackListQuery::MatchType::Substring,
            "Fixture Artist", TrackSortType::Album);
        if (!roundTrip(search)) return false;

        const auto rows = search.GetResult();
        const auto headers = search.GetHeaders();
        const auto durations = search.GetDurations();
        if (!rows || rows->Count() != 2 || !headers || *headers != std::set<size_t>{ 0 } ||
            !durations || durations->count(0) != 1 || durations->at(0) != 2) return false;

        /* The response is keyed by ID. Keep search ordering when reading the batch. */
        constexpr auto missingId = std::numeric_limits<int64_t>::max();
        const auto firstId = rows->GetId(0);
        const auto secondId = rows->GetId(1);
        constexpr auto testRating = 4;
        const auto rating = std::make_shared<SetTrackRatingQuery>(firstId, testRating);
        if (local->EnqueueAndWait(rating) < 0 || rating->GetStatus() != db::IQuery::Finished) return false;

        TrackMetadataBatchQuery batch({ firstId, missingId, secondId }, remote);
        if (!roundTrip(batch)) return false;

        const auto& tracks = batch.Result();
        if (tracks.size() != 2 || tracks.count(missingId) || !tracks.count(firstId) ||
            !tracks.count(secondId)) return false;

        const auto matches = [](const TrackPtr& track, const char* title, const char* number) {
            return track->GetString(MC_TRACK_FIELD_TITLE) == title &&
                track->GetString(MC_TRACK_FIELD_TRACK) == number &&
                track->GetString(MC_TRACK_FIELD_ARTIST) == "Fixture Artist" &&
                track->GetString(MC_TRACK_FIELD_ALBUM) == "Fixture Album" &&
                track->GetString(MC_TRACK_FIELD_DURATION) == "1";
        };
        return firstId == tracks.at(firstId)->GetId() &&
            tracks.at(firstId)->GetString(MC_TRACK_FIELD_RATING) == std::to_string(testRating) &&
            matches(tracks.at(firstId), "Morning Light", "1") &&
            matches(tracks.at(secondId), "Evening Flight", "2");
    }

    bool checkSelectedBackend() {
        using namespace musik::core::api;

        using namespace musik::core;
        library::MasterLibrary selection;
        const auto library = selection.OpenApiLibrary();
        if (!library) return false;
        mc_result completed = MC_OK;
        TrackResult tracks;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.wakeup = query_test_wakeup;
        Context context(options);
        const auto wakes = query_test_wakeup_count();

        /* A retained handle keeps its local backend when the app selects remote. */
        const auto preferences = Preferences::ForComponent(prefs::components::Settings);
        preferences->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
        selection.LoadDefaultLibrary();
        const bool selectedRemote = selection.GetType() == ILibrary::Type::Remote;
        preferences->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
        if (!selectedRemote) return false;

        OpenTrackSearch(context, *library, TrackSearch{ "Fixture Artist" },
            [&](mc_result status, TrackResult result, std::string_view) {
                if (status == MC_OK) tracks = std::move(result);
            });
        if (!query_test_wait_for_wakeup(wakes + 1) || context.Dispatch() != MC_OK || tracks.Size() != 2) return false;

        /* Closing the origin rejects reads even while its database file is present. */
        selection.LoadDefaultLibrary();
        selection.Close();
        OpenTrackSearch(context, *library, TrackSearch{ "Fixture Artist" },
            [&](mc_result status, TrackResult, std::string_view) { completed = status; });
        return query_test_wait_for_wakeup(wakes + 2) && context.Dispatch() == MC_OK && completed == MC_ERROR_IO;
    }
#endif

}

int main(int argc, char **argv) {
#ifdef MUSIKCORE_TEST_APP_BINDING
    struct ShutdownLibraries {
        ~ShutdownLibraries() { musik::core::LibraryFactory::Shutdown(); }
    } shutdown;
#endif
    try {
        if (argc == 2 && checkQuery(argv[1]) && checkSearch(argv[1]) && checkResultWindows(argv[1])) {
#ifdef MUSIKCORE_TEST_APP_BINDING
            if (!checkPlaybackCommands()) {
                std::fputs("Playback command contract failed\n", stderr);
                return 1;
            }
            if (!checkRemoteQueryPayloads()) {
                std::fputs("Remote search and metadata payload contract failed\n", stderr);
                return 1;
            }
            if (!checkSelectedBackend()) {
                std::fputs("Selected library contract failed\n", stderr);
                return 1;
            }
#endif
            return 0;
        }
        std::fputs("C++ wrapper query contract failed\n", stderr);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "C++ wrapper query failed: %s\n", error.what());
    }
    return 1;
}
