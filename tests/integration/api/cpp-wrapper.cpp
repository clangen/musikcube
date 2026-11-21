#include <musikcore/musikcore.hpp>

#ifdef MUSIKCORE_TEST_APP_BINDING
#include "../../support/TestTransport.h"
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/ffi/consumer/playback-binding.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>
#include <musikcore/plugin/Plugins.h>
#include "../../support/CoreRuntime.h"
#include "../../support/ApiHost.h"
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <limits>
#include <set>
#endif

#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>
#include <type_traits>
#include <utility>

extern "C" void MC_CALL query_test_wakeup(void *);
extern "C" int query_test_wait_for_wakeup(unsigned count);
extern "C" unsigned query_test_wakeup_count();
extern "C" void query_test_release_wakeup(void);
bool query_test_dispatch_until(mc_context*, const std::function<bool()>&);

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

    /* Open the same fixture through either the public standalone identity or the production library adapter. */
    std::shared_ptr<const Library> openLibrary(const char *directory) {
#ifdef MUSIKCORE_TEST_APP_BINDING
        musik::core::library::MasterLibrary selection;
        return selection.OpenApiLibrary();
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

        return query_test_dispatch_until(context.Get(), [&] { return !window.Pending(); }) &&
            changes == before + 1 && latest == MC_OK &&
            window.CachedRow(0) && window.CachedRow(1);
    }
#ifdef MUSIKCORE_TEST_APP_BINDING
    /* Reenter the queue during payload destruction to catch cleanup-under-lock deadlocks and delivery to
       unregistered targets. */
    bool checkMessageCleanup() {
        using namespace musik::core::runtime;
        MessageQueue queue;
        struct Target final : IMessageTarget {
            /* Stay inert so the case observes cleanup/reentrancy, not a handler side effect. */
            void ProcessMessage(IMessage&) override {}
        } target, nested;
        queue.Register(&target);
        queue.Register(&nested);
        bool destroyed = false;
        struct Cleanup final : Message {
            /* Keep the queue and peer alive until payload destruction can reenter Unregister. */
            Cleanup(MessageQueue& queue, IMessageTarget* target, IMessageTarget& nested, bool& destroyed)
                : Message(target, 1, 0, 0), queue(queue), nested(nested), destroyed(destroyed) {}

            /* Real payload owners can unregister dependencies during teardown;
               doing this under the queue mutex used to deadlock. */
            ~Cleanup() {
                this->queue.Unregister(&this->nested);
                this->destroyed = true;
            }

            MessageQueue& queue;
            IMessageTarget& nested;
            bool& destroyed;
        };
        queue.Post(std::make_shared<Cleanup>(queue, &target, nested, destroyed));
        const bool passed = queue.Remove(&target) == 1 && destroyed && !queue.Contains(&target);
        queue.Unregister(&target);

        /* Both messages become dispatch-ready together. The first handler
           unregisters the second to expose stale target checks made too early. */
        struct Counter final : IMessageTarget {
            /* Any invocation means an unregistered target still received its batched message. */
            void ProcessMessage(IMessage&) override { ++this->calls; }
            unsigned calls = 0;
        } counter;
        struct Unsubscriber final : IMessageTarget {
            /* Reference the test-owned peer so the first callback can unregister the second target. */
            Unsubscriber(MessageQueue& queue, IMessageTarget& peer) : queue(queue), peer(peer) {}

            /* Invalidate a later target from inside dispatch, outside the queue's signal lock. */
            void ProcessMessage(IMessage&) override { this->queue.Unregister(&this->peer); }

            MessageQueue& queue;
            IMessageTarget& peer;
        } first(queue, counter);
        queue.Register(&first);
        queue.Register(&counter);
        queue.Post(Message::Create(&first, 1));
        queue.Post(Message::Create(&counter, 1));
        queue.Dispatch();
        queue.Unregister(&first);

        /* Destruction with undelivered messages must release their payloads
           while queue members still exist for any reentrant unregister calls. */
        bool releasedAtDestruction = false;
        Target pendingTarget, pendingPeer;
        {
            MessageQueue retiring;
            retiring.Register(&pendingTarget);
            retiring.Register(&pendingPeer);
            retiring.Post(std::make_shared<Cleanup>(retiring, &pendingTarget, pendingPeer, releasedAtDestruction));
        }
        return passed && counter.calls == 0 && releasedAtDestruction;
    }

    /* Report the failing playback assertion's source line without hiding the violated contract behind a
       crash. */
    bool playbackFailure(int line) {
        std::fprintf(stderr, "Playback API contract failed at line %d\n", line);
        return false;
    }

    /* Drive the real playback adapter with fake output so stale row actions, thread violations, and
       queue-state drift are detected without hardware. */
    bool checkPlaybackCommands() {
        using namespace musik::core;
        const auto selection = std::make_shared<library::MasterLibrary>();
        const auto source = selection->OpenApiLibrary();
        if (!source) return playbackFailure(__LINE__);
        api::TrackResult result;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.wakeup = query_test_wakeup;
        api::Context context(options);
        const auto wakes = query_test_wakeup_count();
        api::OpenTrackSearch(context, *source, api::TrackSearch{ "Fixture Artist" },
            [&](mc_result status, api::TrackResult tracks, std::string_view) {
                if (status == MC_OK) result = std::move(tracks);
            });
        if (!query_test_wait_for_wakeup(wakes + 1) || context.Dispatch() != MC_OK || result.Size() != 2) return playbackFailure(__LINE__);

        runtime::MessageQueue queue;
        const auto transport = std::make_shared<TestTransport>();
        Preferences::ForComponent(musik::core::prefs::components::Settings)
            ->SetBool(musik::core::prefs::keys::SaveSessionOnExit, false);
        std::optional<api::Playback> playback;
        {
            audio::PlaybackService service(queue, selection, transport);
            queue.Dispatch();
            playback.emplace(musik::core::audio::CreatePlaybackHandle(service));
            mc_play_queue* rawQueue = nullptr;
            if (mc_playback_get_queue(playback->Get(), &rawQueue) != MC_OK) return playbackFailure(__LINE__);
            const auto originalRaw = rawQueue;
            const bool preservedOutput = mc_playback_get_queue(playback->Get(), &rawQueue) == MC_ERROR_INVALID_ARGUMENT &&
                rawQueue == originalRaw;
            mc_play_queue_destroy(&rawQueue);
            if (!preservedOutput || rawQueue) return playbackFailure(__LINE__);

            /* Default snapshots, bounds, and every transport-state mapping cross the C surface. */
            mc_playback_snapshot invalidSnapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
            invalidSnapshot.struct_size = 0;
            if (mc_playback_get_snapshot(playback->Get(), &invalidSnapshot) != MC_ERROR_INVALID_ARGUMENT ||
                invalidSnapshot.struct_size || invalidSnapshot.volume) return playbackFailure(__LINE__);
            for (const auto state : {sdk::PlaybackState::Stopped, sdk::PlaybackState::Paused,
                sdk::PlaybackState::Prepared, sdk::PlaybackState::Playing}) {
                transport->state = state;
                const auto stateSnapshot = playback->Snapshot();
                const auto expectedState = state == sdk::PlaybackState::Stopped ? MC_PLAYBACK_STOPPED :
                    state == sdk::PlaybackState::Paused ? MC_PLAYBACK_PAUSED :
                    state == sdk::PlaybackState::Prepared ? MC_PLAYBACK_PREPARED : MC_PLAYBACK_PLAYING;
                if (!stateSnapshot || stateSnapshot->state != expectedState) return playbackFailure(__LINE__);
            }
            if (playback->Control(MC_PLAYBACK_SET_VOLUME, std::numeric_limits<double>::quiet_NaN()) != MC_ERROR_INVALID_ARGUMENT ||
                playback->Control(MC_PLAYBACK_SET_POSITION, std::numeric_limits<double>::infinity()) != MC_ERROR_INVALID_ARGUMENT ||
                playback->Control(MC_PLAYBACK_SET_POSITION, -1) != MC_ERROR_INVALID_ARGUMENT ||
                playback->Control(UINT32_MAX) != MC_ERROR_UNSUPPORTED) return playbackFailure(__LINE__);
            const auto empty = playback->Queue();
            if (!empty || empty->Size() || empty->CurrentRow() != SIZE_MAX ||
                playback->PlayQueueRow(*empty, 0) != MC_ERROR_NOT_FOUND) return playbackFailure(__LINE__);
            if (playback->Control(MC_PLAYBACK_TOGGLE_PAUSE) != MC_OK ||
                transport->state != sdk::PlaybackState::Paused ||
                playback->Control(MC_PLAYBACK_SET_VOLUME, 0.25) != MC_OK ||
                transport->volume != 0.25 ||
                playback->Control(MC_PLAYBACK_SET_VOLUME, 1.5) != MC_ERROR_INVALID_ARGUMENT ||
                playback->Control(MC_PLAYBACK_STOP) != MC_OK ||
                transport->state != sdk::PlaybackState::Stopped) return playbackFailure(__LINE__);

            if (playback->Control(MC_PLAYBACK_SET_POSITION, 12) != MC_OK ||
                playback->Control(MC_PLAYBACK_TOGGLE_MUTE) != MC_OK) return playbackFailure(__LINE__);
            const auto snapshot = playback->Snapshot();
            if (!snapshot || snapshot->state != MC_PLAYBACK_STOPPED || snapshot->volume != 0.25 ||
                snapshot->position != 12 || snapshot->duration != 30 || !snapshot->muted) return playbackFailure(__LINE__);

            /* Wrong-thread access must retain its diagnosis rather than appear absent. */
            bool snapshotBusy = false;
            bool queueBusy = false;
            std::thread observer([&] {
                try { playback->Snapshot(); }
                catch (const api::Error& error) { snapshotBusy = error.Result() == MC_ERROR_BUSY; }
                try { playback->Queue(); }
                catch (const api::Error& error) { queueBusy = error.Result() == MC_ERROR_BUSY; }
            });
            observer.join();
            if (!snapshotBusy || !queueBusy) return playbackFailure(__LINE__);

            if (playback->Play(result, 2) != MC_ERROR_NOT_FOUND ||
                playback->Play(result, 1) != MC_OK || service.GetIndex() != 1 ||
                service.GetTrackList()->GetId(0) != result.Id(0) ||
                service.GetTrackList()->GetId(1) != result.Id(1)) return playbackFailure(__LINE__);
            const auto original = playback->Queue();
            if (!original || playback->Move(*original, 1, 0) != MC_OK ||
                service.GetTrackList()->GetId(0) != result.Id(1) ||
                playback->Remove(*original, 1) != MC_ERROR_NOT_FOUND) return playbackFailure(__LINE__);
            const auto reordered = playback->Queue();
            if (!reordered || playback->Remove(*reordered, 1) != MC_OK || service.Count() != 1 ||
                playback->Remove(*reordered, 1) != MC_ERROR_NOT_FOUND ||
                playback->ClearQueue() != MC_OK || service.Count() != 0) return playbackFailure(__LINE__);

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
                service.Count() != 4 || trackList->GetId(0) != second.Id()) return playbackFailure(__LINE__);

            if (playback->UseTrack({}, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_INVALID_ARGUMENT ||
                playback->UseTrack(first, 0) != MC_ERROR_UNSUPPORTED) return playbackFailure(__LINE__);

            /* Snapshots preserve duplicate IDs and remain valid across subsequent edits. */
            const auto captured = playback->Queue();
            const std::array expected{ second.Id(), first.Id(), second.Id(), first.Id() };
            const auto captureMatches = [&] {
                if (!captured) return playbackFailure(__LINE__);
                size_t count = 0;
                const auto ids = captured->Ids(count);
                return count == expected.size() && std::equal(expected.begin(), expected.end(), ids);
            };
            if (!captureMatches() ||
                captured->IsShuffled() || playback->PlayQueueRow(*captured, 2) != MC_OK || service.GetIndex() != 2 ||
                empty->Size()) return playbackFailure(__LINE__);

            if (playback->Control(MC_PLAYBACK_TOGGLE_SHUFFLE) != MC_OK) return playbackFailure(__LINE__);
            const auto shuffled = playback->Queue();
            if (!shuffled || !shuffled->IsShuffled() || shuffled->Size() != expected.size()) return playbackFailure(__LINE__);
            size_t shuffledCount = 0;
            const auto shuffledIds = shuffled->Ids(shuffledCount);
            std::vector<int64_t> sortedActual(shuffledIds, shuffledIds + shuffledCount);
            std::vector<int64_t> sortedExpected(expected.begin(), expected.end());
            std::sort(sortedActual.begin(), sortedActual.end());
            std::sort(sortedExpected.begin(), sortedExpected.end());
            if (sortedActual != sortedExpected || playback->Control(MC_PLAYBACK_TOGGLE_SHUFFLE) != MC_OK) return playbackFailure(__LINE__);
            auto restored = playback->Queue();
            if (!restored || restored->IsShuffled() || playback->PlayQueueRow(*restored, 2) != MC_OK ||
                !captureMatches()) return playbackFailure(__LINE__);
            size_t restoredCount = 0;
            const auto restoredIds = restored->Ids(restoredCount);
            auto movedQueue = std::move(*restored);
            size_t movedCount = 0;
            if (restored->Size() || movedQueue.Ids(movedCount) != restoredIds ||
                movedCount != expected.size() || !std::equal(expected.begin(), expected.end(), restoredIds)) return playbackFailure(__LINE__);

            /* A legacy writer can swap duplicate entries without changing the visible ID sequence. */
            {
                auto edit = service.Edit();
                if (!edit.Swap(0, 2) || edit.Delete(expected.size())) return playbackFailure(__LINE__);
            }
            if (playback->PlayQueueRow(*captured, 0) != MC_ERROR_NOT_FOUND ||
                playback->Move(*captured, 0, 1) != MC_ERROR_NOT_FOUND ||
                playback->Remove(*captured, 0) != MC_ERROR_NOT_FOUND ||
                service.Count() != expected.size() || service.GetIndex() != 2) return playbackFailure(__LINE__);
            const auto current = playback->Queue();
            if (!current || playback->PlayQueueRow(*current, 2) != MC_OK) return playbackFailure(__LINE__);

            const auto prefs = Preferences::ForComponent(musik::core::prefs::components::Settings);
            prefs->SetInt(musik::core::prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
            selection->LoadDefaultLibrary();
            const auto unavailable = playback->Queue();
            const bool rejected = unavailable && !unavailable->Size() &&
                playback->PlayQueueRow(*current, 0) == MC_ERROR_NOT_FOUND &&
                playback->Remove(*current, 0) == MC_ERROR_NOT_FOUND &&
                playback->Play(result, 0) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NOW) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NEXT) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_APPEND) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PREPEND) == MC_ERROR_NOT_FOUND &&
                service.Count() == 4 && trackList->GetId(0) == second.Id();
            audio::DiscardUnavailablePlaybackQueue(service);
            const bool cleared = service.Count() == 0 && transport->state == sdk::PlaybackState::Stopped;
            prefs->SetInt(musik::core::prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
            selection->LoadDefaultLibrary();
            if (!rejected || !cleared || !captureMatches()) return playbackFailure(__LINE__);

            /* Reenter subscription from the real dispatcher and destroy a peer with pending delivery. */
            queue.Dispatch();
            struct Observers {
                audio::PlaybackService& service;
                api::Playback& primary;
                std::optional<api::Playback> peer;
                unsigned primaryCalls = 0;
                unsigned cancelledCalls = 0;
                unsigned replacementCalls = 0;
                mc_result subscribed = MC_ERROR_INTERNAL;
                mc_result unsubscribed = MC_ERROR_INTERNAL;
            } observers{service, *playback, audio::CreatePlaybackHandle(service)};
            const auto countCall = [](void* data) { ++*static_cast<unsigned*>(data); };
            const auto replacePeer = [](void* data) {
                auto& state = *static_cast<Observers*>(data);
                ++state.primaryCalls;
                state.peer.reset();
                state.peer.emplace(audio::CreatePlaybackHandle(state.service));
                state.subscribed = state.peer->SetQueueChangedCallback(
                    [](void* count) { ++*static_cast<unsigned*>(count); }, &state.replacementCalls);
                state.unsubscribed = state.primary.SetQueueChangedCallback(nullptr);
            };

            if (playback->SetQueueChangedCallback(replacePeer, &observers) != MC_OK ||
                observers.peer->SetQueueChangedCallback(countCall, &observers.cancelledCalls) != MC_OK ||
                playback->ClearQueue() != MC_OK || playback->ClearQueue() != MC_OK) return playbackFailure(__LINE__);
            queue.Dispatch();
            if (observers.primaryCalls || observers.cancelledCalls) return playbackFailure(__LINE__);
            queue.Dispatch();
            if (observers.primaryCalls != 1 || observers.cancelledCalls || observers.replacementCalls ||
                observers.subscribed != MC_OK || observers.unsubscribed != MC_OK ||
                playback->ClearQueue() != MC_OK) return playbackFailure(__LINE__);
            queue.Dispatch();
            queue.Dispatch();
            if (observers.replacementCalls != 1 || playback->ClearQueue() != MC_OK) return playbackFailure(__LINE__);
            queue.Dispatch();
            if (observers.peer->SetQueueChangedCallback(nullptr) != MC_OK) return playbackFailure(__LINE__);
            queue.Dispatch();
            if (observers.replacementCalls != 1) return playbackFailure(__LINE__);

            /* Load through the typed playlist query with playback stopped. */
            const std::vector savedIds{ second.Id(), first.Id(), second.Id(), first.Id(), first.Id() };
            constexpr auto playlistName = "Queue Load Fixture";
            auto wake = query_test_wakeup_count();
            int64_t playlistId = 0;
            api::SavePlaylist(context, *source, 0, playlistName, { first.Id() },
                [&](mc_result status, int64_t id, std::string_view) { if (status == MC_OK) playlistId = id; });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || playlistId <= 0) return playbackFailure(__LINE__);

            wake = query_test_wakeup_count();
            bool listed = false;
            api::ListPlaylists(context, *source, [&](mc_result status, auto entries, std::string_view) {
                listed = status == MC_OK && std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
                    return entry.id == playlistId && entry.name == playlistName;
                });
            });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || !listed) return playbackFailure(__LINE__);

            wake = query_test_wakeup_count();
            bool overwritten = false;
            api::SavePlaylist(context, *source, playlistId, {}, { second.Id(), first.Id(), second.Id() },
                [&](mc_result status, int64_t id, std::string_view) { overwritten = status == MC_OK && id == playlistId; });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || !overwritten) return playbackFailure(__LINE__);

            wake = query_test_wakeup_count();
            bool appended = false;
            api::AppendPlaylist(context, *source, playlistId, { first.Id(), first.Id() },
                [&](mc_result status, int64_t id, std::string_view) { appended = status == MC_OK && id == playlistId; });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || !appended) return playbackFailure(__LINE__);

            mc_result loaded = MC_ERROR_INTERNAL;
            const auto load = [&](mc_result status, api::TrackResult tracks, std::string_view) {
                loaded = status == MC_OK ? playback->ReplaceQueue(tracks) : status;
            };
            wake = query_test_wakeup_count();
            api::OpenPlaylist(context, *source, playlistId, load);
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || loaded != MC_OK ||
                transport->state != sdk::PlaybackState::Stopped) return playbackFailure(__LINE__);
            const auto loadedQueue = playback->Queue();
            size_t loadedCount = 0;
            const auto ids = loadedQueue ? loadedQueue->Ids(loadedCount) : nullptr;
            if (loadedCount != savedIds.size() || !std::equal(savedIds.begin(), savedIds.end(), ids)) return playbackFailure(__LINE__);

            /* Stop retains the row for selection and Play Next, without a playing highlight. */
            if (playback->PlayQueueRow(*loadedQueue, 0) != MC_OK ||
                playback->Control(MC_PLAYBACK_STOP) != MC_OK) return playbackFailure(__LINE__);
            const auto stopped = playback->Queue();
            if (!stopped || stopped->RetainedRow() != 0 || stopped->CurrentRow() != SIZE_MAX ||
                playback->UseTrack(second, MC_PLAYBACK_TRACK_PLAY_NEXT) != MC_OK ||
                trackList->GetId(1) != second.Id() || trackList->GetId(2) != first.Id()) return playbackFailure(__LINE__);

            /* Switch after the request becomes ready but before dispatching its result. */
            wake = query_test_wakeup_count();
            api::OpenPlaylist(context, *source, playlistId, load);
            if (!query_test_wait_for_wakeup(wake + 1)) return playbackFailure(__LINE__);
            prefs->SetInt(musik::core::prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
            selection->LoadDefaultLibrary();
            const bool ignored = context.Dispatch() == MC_OK && loaded == MC_ERROR_NOT_FOUND &&
                service.Count() == savedIds.size() + 1;
            prefs->SetInt(musik::core::prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
            selection->LoadDefaultLibrary();
            if (!ignored) return playbackFailure(__LINE__);

            /* Rename and delete the selected playlist, leaving the active queue alone. */
            constexpr auto renamedName = "Renamed Queue Fixture";
            wake = query_test_wakeup_count();
            mc_result mutation = MC_ERROR_INTERNAL;
            const auto mutated = [&](mc_result status, int64_t id, std::string_view) {
                mutation = id == playlistId ? status : MC_ERROR_NOT_FOUND;
            };
            api::RenamePlaylist(context, *source, playlistId, renamedName, mutated);
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || mutation != MC_OK) return playbackFailure(__LINE__);
            wake = query_test_wakeup_count();
            listed = false;
            api::ListPlaylists(context, *source, [&](mc_result status, auto entries, std::string_view) {
                listed = status == MC_OK && std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
                    return entry.id == playlistId && entry.name == renamedName;
                });
            });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || !listed) return playbackFailure(__LINE__);
            wake = query_test_wakeup_count();
            mutation = MC_ERROR_INTERNAL;
            api::DeletePlaylist(context, *source, playlistId, mutated);
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || mutation != MC_OK ||
                service.Count() != savedIds.size() + 1) return playbackFailure(__LINE__);
            wake = query_test_wakeup_count();
            bool removed = false;
            api::ListPlaylists(context, *source, [&](mc_result status, auto entries, std::string_view) {
                removed = status == MC_OK && std::none_of(entries.begin(), entries.end(),
                    [&](const auto& entry) { return entry.id == playlistId; });
            });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || !removed) return playbackFailure(__LINE__);

            mc_result otherThread = MC_OK;
            std::optional<api::Playback> otherHandle;
            std::thread caller([&] {
                otherHandle.emplace(musik::core::audio::CreatePlaybackHandle(service));
                otherThread = otherHandle->Control(MC_PLAYBACK_STOP);
            });
            caller.join();
            if (otherThread != MC_ERROR_BUSY || otherHandle->Control(MC_PLAYBACK_STOP) != MC_OK) return playbackFailure(__LINE__);

            /* A fresh service has no queued edit events from earlier scenarios. */
            runtime::MessageQueue stateMessages;
            const auto stateTransport = std::make_shared<TestTransport>();
            audio::PlaybackService stateService(stateMessages, selection, stateTransport);
            stateMessages.Dispatch();
            auto statePlayback = audio::CreatePlaybackHandle(stateService);
            struct LiveEvents {
                std::vector<mc_playback_event> received;
            } live;
            const auto record = [](void* data, mc_playback_event event) {
                auto& events = *static_cast<LiveEvents*>(data);
                events.received.push_back(event);
            };
            mc_result subscribed = MC_ERROR_INTERNAL;
            std::thread subscribe([&] { subscribed = statePlayback.SetChangedCallback(record, &live); });
            subscribe.join();
            if (subscribed != MC_OK) {
                return playbackFailure(__LINE__);
            }
            constexpr auto invalidEvent = static_cast<mc_playback_event>(0);
            if (mc_playback_set_changed_callback(statePlayback.Get(), record, &live, nullptr, 1) !=
                    MC_ERROR_INVALID_ARGUMENT ||
                mc_playback_set_changed_callback(statePlayback.Get(), record, &live, &invalidEvent, 1) !=
                    MC_ERROR_INVALID_ARGUMENT)
            {
                return playbackFailure(__LINE__);
            }

            LiveEvents scoped;
            const auto owner = std::this_thread::get_id();
            bool deliveredOnOwner = true;
            api::PlaybackEventSubscription subscriber(audio::CreatePlaybackHandle(stateService),
                [&](mc_playback_event event) {
                    deliveredOnOwner &= std::this_thread::get_id() == owner;
                    record(&scoped, event);
                });
            LiveEvents filtered;
            std::vector filter { MC_PLAYBACK_CHANGED_TRACK, MC_PLAYBACK_CHANGED_VOLUME };
            api::PlaybackEventSubscription filteredSubscriber(audio::CreatePlaybackHandle(stateService),
                [&](mc_playback_event event) { record(&filtered, event); }, filter);
            std::fill(filter.begin(), filter.end(), MC_PLAYBACK_CHANGED_MODE);

            unsigned destroyedCalls = 0;
            auto destroyed = std::make_unique<api::PlaybackEventSubscription>(audio::CreatePlaybackHandle(stateService),
                [&](mc_playback_event) { ++destroyedCalls; });

            if (statePlayback.Play(result, 1) != MC_OK) return playbackFailure(__LINE__);
            stateTransport->state = sdk::PlaybackState::Prepared;
            stateTransport->PlaybackEvent(sdk::PlaybackState::Prepared);
            stateTransport->stream = sdk::StreamState::Buffered;
            stateTransport->StreamEvent(sdk::StreamState::Buffered, stateTransport->Uri());
            stateTransport->VolumeChanged();
            stateTransport->VolumeChanged();
            stateTransport->TimeChanged(0);
            stateService.SetRepeatMode(sdk::RepeatMode::Track);

            /* Deliver each distinct event once, with the registration filter copied at subscribe time. */
            stateMessages.Dispatch();
            if (!live.received.empty() || !scoped.received.empty() || !filtered.received.empty() || destroyedCalls) {
                return playbackFailure(__LINE__);
            }
            std::thread destroy([subscription = std::move(destroyed)]() mutable { subscription.reset(); });
            destroy.join();
            stateMessages.Dispatch();
            const std::vector allEvents {
                MC_PLAYBACK_CHANGED_TRACK, MC_PLAYBACK_CHANGED_STATE, MC_PLAYBACK_CHANGED_STREAM,
                MC_PLAYBACK_CHANGED_VOLUME, MC_PLAYBACK_CHANGED_POSITION, MC_PLAYBACK_CHANGED_MODE
            };
            const std::vector filteredEvents { MC_PLAYBACK_CHANGED_TRACK, MC_PLAYBACK_CHANGED_VOLUME };
            const auto liveSnapshot = statePlayback.Snapshot();
            if (live.received != allEvents || scoped.received != allEvents || destroyedCalls ||
                filtered.received != filteredEvents ||
                !liveSnapshot || liveSnapshot->stream != MC_PLAYBACK_STREAM_BUFFERED ||
                liveSnapshot->repeat != MC_PLAYBACK_REPEAT_TRACK)
            {
                return playbackFailure(__LINE__);
            }

            /* Replace a queued callback from another thread and drop its delivery. */
            stateTransport->VolumeChanged();
            stateMessages.Dispatch();
            LiveEvents replacement;
            std::thread replace([&] { subscribed = statePlayback.SetChangedCallback(record, &replacement); });
            replace.join();
            stateMessages.Dispatch();
            if (subscribed != MC_OK || live.received != allEvents || !replacement.received.empty() ||
                scoped.received.size() != allEvents.size() + 1)
            {
                return playbackFailure(__LINE__);
            }

            std::unique_ptr<api::PlaybackEventSubscription> self;
            size_t selfCalls = 0;
            self = std::make_unique<api::PlaybackEventSubscription>(audio::CreatePlaybackHandle(stateService),
                [&](mc_playback_event) {
                    self.reset();
                    ++selfCalls;
                });
            stateTransport->VolumeChanged();
            stateTransport->TimeChanged(0);
            stateMessages.Dispatch();
            stateMessages.Dispatch();
            const std::vector selfBatch { MC_PLAYBACK_CHANGED_VOLUME, MC_PLAYBACK_CHANGED_POSITION };
            if (self || selfCalls != 1 || replacement.received != selfBatch ||
                scoped.received.size() != allEvents.size() + 3)
            {
                return playbackFailure(__LINE__);
            }

            /* A worker can unsubscribe while delivery is in progress. It must wait for the callback. */
            struct InFlight {
                std::mutex mutex;
                std::condition_variable changed;
                bool entered = false;
                bool attempting = false;
                bool returned = false;
                bool waited = false;
                size_t calls = 0;
            } inFlight;
            const auto block = [](void* data, mc_playback_event) {
                auto& state = *static_cast<InFlight*>(data);
                std::unique_lock lock(state.mutex);
                ++state.calls;
                state.entered = true;
                state.changed.notify_one();
                if (!state.changed.wait_for(lock, std::chrono::seconds(2), [&] { return state.attempting; })) {
                    return;
                }
                state.waited = !state.changed.wait_for(lock, std::chrono::milliseconds(30),
                    [&] { return state.returned; });
            };
            if (statePlayback.SetChangedCallback(block, &inFlight) != MC_OK) {
                return playbackFailure(__LINE__);
            }
            stateTransport->VolumeChanged();
            stateMessages.Dispatch();
            std::thread unsubscribe([&] {
                {
                    std::unique_lock lock(inFlight.mutex);
                    if (!inFlight.changed.wait_for(lock, std::chrono::seconds(2), [&] { return inFlight.entered; })) {
                        return;
                    }
                    inFlight.attempting = true;
                    inFlight.changed.notify_one();
                }
                subscribed = statePlayback.SetChangedCallback(nullptr);
                {
                    std::lock_guard lock(inFlight.mutex);
                    inFlight.returned = true;
                    inFlight.changed.notify_one();
                }
            });
            stateMessages.Dispatch();
            unsubscribe.join();
            stateTransport->VolumeChanged();
            stateMessages.Dispatch();
            stateMessages.Dispatch();
            if (subscribed != MC_OK || !inFlight.waited || !inFlight.returned ||
                inFlight.calls != 1 || scoped.received.size() != allEvents.size() + 5 ||
                selfCalls != 1 || !deliveredOnOwner)
            {
                return playbackFailure(__LINE__);
            }

            for (const auto state : {sdk::PlaybackState::Paused, sdk::PlaybackState::Prepared, sdk::PlaybackState::Playing}) {
                stateTransport->state = state;
                const auto stateQueue = statePlayback.Queue();
                const auto current = statePlayback.CurrentTrack();
                if (!stateQueue || stateQueue->CurrentRow() != 1 || stateQueue->RetainedRow() != 1 ||
                    !current || current->Id() != result.Id(1))
                {
                    return playbackFailure(__LINE__);
                }
            }
            const auto retainedTrack = statePlayback.CurrentTrack();
            auto engineTrack = stateService.GetPlaying();
            const auto initialGain = engineTrack->GetReplayGain();
            engineTrack->SetReplayGain({ 1.0f, 0.5f, 1.0f, 1.0f });
            const auto gainSnapshot = statePlayback.Snapshot();
            engineTrack->SetReplayGain(initialGain);
            if (!gainSnapshot || !gainSnapshot->has_replay_gain) {
                return playbackFailure(__LINE__);
            }
            if (!retainedTrack || statePlayback.Control(MC_PLAYBACK_STOP) != MC_OK) {
                return playbackFailure(__LINE__);
            }
            const auto stoppedBeforeNotification = statePlayback.Queue();
            if (!stoppedBeforeNotification || stoppedBeforeNotification->CurrentRow() != SIZE_MAX ||
                stoppedBeforeNotification->RetainedRow() != 1 || !stateService.GetPlaying() ||
                statePlayback.CurrentTrack())
            {
                return playbackFailure(__LINE__);
            }
            const auto trackWake = query_test_wakeup_count();
            bool readCurrent = false;
            api::ReadTrack(context, *retainedTrack, { MC_TRACK_FIELD_ALBUM_ID },
                [&](mc_result status, api::TrackPage page, std::string_view) {
                    readCurrent = status == MC_OK && page.Size() == 1 && page.Id(0) == retainedTrack->Id() &&
                        !page.Value(0, 0).empty();
                });
            if (!query_test_wait_for_wakeup(trackWake + 1) || context.Dispatch() != MC_OK || !readCurrent) {
                return playbackFailure(__LINE__);
            }
            if (statePlayback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK ||
                !AwaitTestHost(stateMessages, [&] { return stateTransport->reloads == 1; }))
            {
                return playbackFailure(__LINE__);
            }
        }
        return playback->Control(MC_PLAYBACK_STOP) == MC_ERROR_NOT_FOUND &&
            !playback->Snapshot() && !playback->Queue() && !playback->CurrentTrack();
    }

    /* Verify typed remote query serialization preserves ordering/grouping and rejects malformed payloads. */
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

    /* Compare category/directory selections and read reordered duplicate IDs through the selected backend. */
    bool checkBrowseReads(const char* directory) {
        using namespace musik::core;
        library::MasterLibrary selected;
        const auto source = selected.OpenApiLibrary();
        if (!source) return false;

        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.wakeup = query_test_wakeup;
        api::Context context(options);
        api::TrackResult album;
        api::TrackResult artist;
        api::TrackResult folder;
        auto wake = query_test_wakeup_count();
        const auto receive = [](api::TrackResult& result) {
            return [&result](mc_result status, api::TrackResult tracks, std::string_view) {
                if (status == MC_OK) result = std::move(tracks);
            };
        };
        for (const auto& [key, result] : std::array{
            std::pair{MC_TRACK_FIELD_ALBUM, &album}, std::pair{MC_TRACK_FIELD_ARTIST, &artist}}) {
            using Categories = library::query::CategoryListQuery;
            const auto categories = std::make_shared<Categories>(Categories::MatchType::Substring, key);
            if (selected.EnqueueAndWait(categories, kWaitIndefinite) < 0 || !categories->GetResult()->Count()) return false;
            api::OpenCategory(context, *source, key, categories->GetResult()->At(0)->GetId(),
                MC_TRACK_SEARCH_ALBUM, receive(*result));
            if (!query_test_wait_for_wakeup(++wake) || context.Dispatch() != MC_OK ||
                result->Size() != 2 || !result->StartsAlbum(0) || result->StartsAlbum(1) ||
                result->AlbumDurationSeconds(0) != 2) return false;
        }
        const auto path = (std::filesystem::path(directory) / ".." / ".." / "media").lexically_normal();
        context.SetLibrary(*source);
        bool fieldsListed = false;
        api::ListCategoryFields(context, [&](mc_result status, std::vector<std::string> fields, std::string_view) {
            fieldsListed = status == MC_OK &&
                std::ranges::find(fields, MC_TRACK_FIELD_ALBUM) != fields.end() &&
                std::ranges::find(fields, MC_TRACK_FIELD_ARTIST) != fields.end();
        });
        if (!query_test_wait_for_wakeup(++wake) || context.Dispatch() != MC_OK || !fieldsListed) {
            return false;
        }
        api::OpenDirectory(context, *source, path.string(), receive(folder));
        if (!query_test_wait_for_wakeup(++wake) || context.Dispatch() != MC_OK ||
            folder.Size() != 2 || !folder.StartsAlbum(0) || folder.AlbumDurationSeconds(0) != 2 ||
            folder.Id(0) != album.Id(0) || folder.Id(1) != album.Id(1) ||
            artist.Id(0) != album.Id(0) || artist.Id(1) != album.Id(1)) return false;

        const std::array ids{album.Id(1), album.Id(0), album.Id(1)};
        const auto queue = api::MakeTrackResult(*source, ids.data(), ids.size());
        api::TrackPage page;
        api::ReadTracks(context, queue, {0, ids.size(), {MC_TRACK_FIELD_TITLE}},
            [&](mc_result status, api::TrackPage tracks, std::string_view) {
                if (status == MC_OK) page = std::move(tracks);
            });
        return query_test_wait_for_wakeup(++wake) && context.Dispatch() == MC_OK && page.Size() == ids.size() &&
            page.Id(0) == ids[0] && page.Id(1) == ids[1] && page.Id(2) == ids[2] &&
            page.Track(0).Title() == "Evening Flight" && page.Track(1).Title() == "Morning Light" &&
            page.Track(2).Title() == "Evening Flight";
    }

    /* Switch the selected backend while retaining an identity so reads cannot silently use another
       library with colliding IDs. */
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

/* Select one independently reported contract for CTest, or retain the full
   legacy flow when no case is specified. Every invocation owns fresh services. */
int main(int argc, char** argv) {
    if (argc != 2 && (argc != 4 || std::strcmp(argv[2], "--case") != 0)) {
        std::fputs("Expected catalog directory [--case name]\n", stderr);
        return 1;
    }

    const char* selected = argc == 4 ? argv[3] : nullptr;
    /* Only query deliberately blocks the producer hook. Other cases must run
       independently instead of relying on query having released it first. */
    if (selected && std::strcmp(selected, "query") != 0) {
        query_test_release_wakeup();
    }
    try {
#ifdef MUSIKCORE_TEST_APP_BINDING
        CoreRuntime runtime;
#endif
        const std::pair<const char*, std::function<bool()>> cases[] = {
            { "query", [&] { return checkQuery(argv[1]); } },
            { "search", [&] { return checkSearch(argv[1]); } },
            { "result_windows", [&] { return checkResultWindows(argv[1]); } },
#ifdef MUSIKCORE_TEST_APP_BINDING
            { "message_cleanup", [] { return checkMessageCleanup(); } },
            { "playback_commands", [] { return checkPlaybackCommands(); } },
            { "remote_payloads", [] { return checkRemoteQueryPayloads(); } },
            { "browse_reads", [&] { return checkBrowseReads(argv[1]); } },
            { "selected_backend", [] { return checkSelectedBackend(); } },
#endif
        };

        bool matched = false;
        for (const auto& entry : cases) {
            if (selected && std::strcmp(selected, entry.first) != 0) {
                continue;
            }
            matched = true;
            if (!entry.second()) {
                std::fprintf(stderr, "C++ wrapper case %s failed\n", entry.first);
                return 1;
            }
        }
        if (!matched) {
            std::fprintf(stderr, "Unknown C++ wrapper case: %s\n", selected);
            return 1;
        }
        return 0;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "C++ wrapper case %s failed: %s\n", selected ? selected : "all", error.what());
        return 1;
    }
}
