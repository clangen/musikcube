#include "cpp-wrapper-cases.h"
#include <musikcore/musikcore.hpp>
#include "../../support/TestTransport.h"
#include "../../support/CoreRuntime.h"
#include "../../support/ApiHost.h"
#include <common/runtime/MessageQueue.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <limits>
#include <mutex>
#include <set>
#include <thread>

namespace wrapper_tests {
    /* Report the failing playback assertion's source line without hiding the violated contract behind a
       crash. */
    static bool playbackFailure(int line) {
        std::fprintf(stderr, "Playback API contract failed at line %d\n", line);
        return false;
    }

    /* Drive the real playback adapter with fake output so stale row actions, thread violations, and
       queue-state drift are detected without hardware. */
    bool checkPlaybackCommands() {
        using namespace musik::core;
        auto& factory = LibraryFactory::Instance();
        const auto local = factory.DefaultLocalLibrary();
        const auto remote = factory.DefaultRemoteLibrary();
        const auto source = OpenFixtureLibrary();
        if (!source) return playbackFailure(__LINE__);
        api::TrackResult result;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = query_test_wakeup;
        api::Context context(options);
        const auto wakes = query_test_wakeup_count();
        api::OpenTrackSearch(context, *source, api::TrackSearch{ "Fixture Artist" },
            [&](mc_result status, api::TrackResult tracks, std::string_view) {
                if (status == MC_OK) result = std::move(tracks);
            });
        if (!query_test_wait_for_wakeup(wakes + 1) || context.Dispatch() != MC_OK || result.Size() != 2) return playbackFailure(__LINE__);

        Preferences::ForComponent(musik::core::prefs::components::Settings)
            ->SetBool(musik::core::prefs::keys::SaveSessionOnExit, false);

        /* hold startup before its playback-state transition so output reload always overlaps the
        pending stream, independently of decoder speed and host scheduling. */
        for (const auto mode : { audio::ITransport::StartMode::Immediate, audio::ITransport::StartMode::Wait }) {
            for (const auto stream : { sdk::StreamState::Buffering, sdk::StreamState::Buffered }) {
                runtime::MessageQueue reloadMessages;
                const auto reloadTransport = std::make_shared<TestTransport>();
                audio::PlaybackService reloadService(reloadMessages, local, reloadTransport);
                reloadMessages.Dispatch();
                auto reloadPlayback = OpenFixturePlayback(reloadService);
                reloadTransport->holdStartup = true;
                if (reloadPlayback.Play(result, 1) != MC_OK) {
                    return playbackFailure(__LINE__);
                }
                if (mode == audio::ITransport::StartMode::Wait) {
                    reloadService.Prepare(1);
                }
                reloadTransport->stream = stream;
                const auto starts = reloadTransport->starts;
                const auto uri = reloadTransport->uri;
                if (reloadPlayback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK ||
                    !AwaitTestHost(reloadMessages, [&] { return reloadTransport->reloads == 1; }) ||
                    reloadTransport->starts != starts + 1 || reloadTransport->uri != uri ||
                    reloadTransport->lastStartMode != mode || reloadService.GetIndex() != 1)
                {
                    std::fprintf(stderr,
                        "startup reload: mode=%d stream=%d reloads=%u starts=%u expected=%u uri_preserved=%d\n",
                        static_cast<int>(mode), static_cast<int>(stream), reloadTransport->reloads,
                        reloadTransport->starts, starts + 1, reloadTransport->uri == uri);
                    return playbackFailure(__LINE__);
                }

                /* an explicit stop retains the queue row but cancels startup intent. */
                if (reloadPlayback.Control(MC_PLAYBACK_STOP) != MC_OK) {
                    return playbackFailure(__LINE__);
                }
                const auto stoppedStarts = reloadTransport->starts;
                if (reloadPlayback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK ||
                    !AwaitTestHost(reloadMessages, [&] { return reloadTransport->reloads == 2; }) ||
                    reloadTransport->starts != stoppedStarts ||
                    reloadTransport->state != sdk::PlaybackState::Stopped)
                {
                    return playbackFailure(__LINE__);
                }
            }
        }

        /* finish startup between the two transport getters. a stale Stopped read must not cause
        the following Playing stream read to discard the requested playback. */
        {
            runtime::MessageQueue reloadMessages;
            const auto reloadTransport = std::make_shared<TestTransport>();
            audio::PlaybackService reloadService(reloadMessages, local, reloadTransport);
            reloadMessages.Dispatch();
            auto reloadPlayback = OpenFixturePlayback(reloadService);
            reloadTransport->holdStartup = true;
            if (reloadPlayback.Play(result, 1) != MC_OK ||
                reloadPlayback.Control(MC_PLAYBACK_RELOAD_OUTPUT) != MC_OK)
            {
                return playbackFailure(__LINE__);
            }
            reloadTransport->completeStartupOnStateRead = true;
            if (!AwaitTestHost(reloadMessages, [&] { return reloadTransport->reloads == 1; }) ||
                reloadTransport->starts != 2 || reloadTransport->uri.empty() ||
                reloadTransport->lastStartMode != audio::ITransport::StartMode::Immediate)
            {
                std::fprintf(stderr, "startup transition reload: starts=%u uri_preserved=%d\n",
                    reloadTransport->starts, !reloadTransport->uri.empty());
                return playbackFailure(__LINE__);
            }
        }

        runtime::MessageQueue queue;
        const auto transport = std::make_shared<TestTransport>();
        std::optional<api::Playback> playback;
        {
            audio::PlaybackService service(queue, local, transport);
            queue.Dispatch();
            playback.emplace(OpenFixturePlayback(service));
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
                catch (const api::Error& error) { snapshotBusy = error.Result() == MC_ERROR_WRONG_THREAD; }
                try { playback->Queue(); }
                catch (const api::Error& error) { queueBusy = error.Result() == MC_ERROR_WRONG_THREAD; }
            });
            observer.join();
            if (!snapshotBusy || !queueBusy) return playbackFailure(__LINE__);

            const auto queueHas = [&](std::initializer_list<int64_t> expected) {
                const auto queue = playback->Queue();
                if (!queue) {
                    return false;
                }

                size_t count = 0;
                const auto ids = queue->Ids(count);
                return count == expected.size() && std::equal(expected.begin(), expected.end(), ids);
            };

            if (playback->Play(result, 2) != MC_ERROR_NOT_FOUND ||
                playback->Play(result, 1) != MC_OK || service.GetIndex() != 1 ||
                !queueHas({ result.Id(0), result.Id(1) })) return playbackFailure(__LINE__);
            const auto original = playback->Queue();
            if (!original || playback->Move(*original, 1, 0) != MC_OK ||
                !queueHas({ result.Id(1), result.Id(0) }) ||
                playback->Remove(*original, 1) != MC_ERROR_STALE_SNAPSHOT) return playbackFailure(__LINE__);
            const auto reordered = playback->Queue();
            if (!reordered || playback->Remove(*reordered, 1) != MC_OK || service.Count() != 1 ||
                playback->Remove(*reordered, 1) != MC_ERROR_STALE_SNAPSHOT ||
                playback->ClearQueue() != MC_OK || service.Count() != 0) return playbackFailure(__LINE__);

            const auto first = result.Track(0);
            const auto second = result.Track(1);
            if (playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NOW) != MC_OK ||
                transport->state != sdk::PlaybackState::Playing ||
                !queueHas({ first.Id() }) ||
                playback->UseTrack(first, MC_PLAYBACK_TRACK_APPEND) != MC_OK ||
                playback->UseTrack(second, MC_PLAYBACK_TRACK_PLAY_NEXT) != MC_OK ||
                !queueHas({ first.Id(), second.Id(), first.Id() }) ||
                playback->UseTrack(second, MC_PLAYBACK_TRACK_PREPEND) != MC_OK ||
                !queueHas({ second.Id(), first.Id(), second.Id(), first.Id() })) return playbackFailure(__LINE__);

            if (playback->UseTrack({}, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_INVALID_ARGUMENT ||
                playback->UseTrack(first, 0) != MC_ERROR_UNSUPPORTED) return playbackFailure(__LINE__);

            /* Snapshots preserve duplicate IDs and remain valid across subsequent edits. */
            const auto captured = playback->Queue();
            const std::array expected{ second.Id(), first.Id(), second.Id(), first.Id() };
            const auto sameRevision = playback->Queue();
            if (!captured || !sameRevision || !sameRevision->SameRows(*captured) ||
                captured->Tracks().Size() != expected.size())
            {
                return playbackFailure(__LINE__);
            }
            const auto projection = captured->Tracks();
            const auto captureMatches = [&] {
                if (!captured) return playbackFailure(__LINE__);
                size_t count = 0;
                const auto ids = captured->Ids(count);
                return count == expected.size() && std::equal(expected.begin(), expected.end(), ids) &&
                    projection.Id(0) == expected[0] && projection.Id(2) == expected[2];
            };
            if (!captureMatches() ||
                captured->IsShuffled() || playback->PlayQueueRow(*captured, 2) != MC_OK || service.GetIndex() != 2 ||
                empty->Size()) return playbackFailure(__LINE__);

            if (playback->Control(MC_PLAYBACK_TOGGLE_SHUFFLE) != MC_OK) return playbackFailure(__LINE__);
            const auto shuffled = playback->Queue();
            if (!shuffled || shuffled->SameRows(*captured) || !shuffled->IsShuffled() || shuffled->Size() != expected.size()) return playbackFailure(__LINE__);
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
            if (playback->PlayQueueRow(*captured, 0) != MC_ERROR_STALE_SNAPSHOT ||
                playback->Move(*captured, 0, 1) != MC_ERROR_STALE_SNAPSHOT ||
                playback->Remove(*captured, 0) != MC_ERROR_STALE_SNAPSHOT ||
                service.Count() != expected.size() || service.GetIndex() != 2) return playbackFailure(__LINE__);
            const auto current = playback->Queue();
            if (!current || playback->PlayQueueRow(*current, 2) != MC_OK) return playbackFailure(__LINE__);

            service.SelectLibrary(remote);
            const auto unavailable = playback->Queue();
            const bool rejected = unavailable && !unavailable->Size() &&
                playback->PlayQueueRow(*current, 0) == MC_ERROR_STALE_SNAPSHOT &&
                playback->Remove(*current, 0) == MC_ERROR_STALE_SNAPSHOT &&
                playback->Play(result, 0) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NOW) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PLAY_NEXT) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_APPEND) == MC_ERROR_NOT_FOUND &&
                playback->UseTrack(first, MC_PLAYBACK_TRACK_PREPEND) == MC_ERROR_NOT_FOUND &&
                service.Count() == 0;
            const bool cleared = service.Count() == 0 && transport->state == sdk::PlaybackState::Stopped;
            service.SelectLibrary(local);
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
            } observers{service, *playback, OpenFixturePlayback(service)};
            const auto countCall = [](void* data) { ++*static_cast<unsigned*>(data); };
            const auto replacePeer = [](void* data) {
                auto& state = *static_cast<Observers*>(data);
                ++state.primaryCalls;
                state.peer.reset();
                state.peer.emplace(OpenFixturePlayback(state.service));
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
                playback->UseTrack(second, MC_PLAYBACK_TRACK_PLAY_NEXT) != MC_OK) return playbackFailure(__LINE__);
            const auto afterPlayNext = playback->Queue();
            size_t afterPlayNextCount = 0;
            const auto afterPlayNextIds = afterPlayNext ? afterPlayNext->Ids(afterPlayNextCount) : nullptr;
            if (afterPlayNextCount < 3 || afterPlayNextIds[1] != second.Id() ||
                afterPlayNextIds[2] != first.Id()) return playbackFailure(__LINE__);

            /* Switch after the request becomes ready but before dispatching its result. */
            wake = query_test_wakeup_count();
            api::OpenPlaylist(context, *source, playlistId, load);
            if (!query_test_wait_for_wakeup(wake + 1)) return playbackFailure(__LINE__);
            service.SelectLibrary(remote);
            const bool ignored = context.Dispatch() == MC_OK && loaded == MC_ERROR_NOT_FOUND &&
                service.Count() == 0;
            service.SelectLibrary(local);
            if (!ignored || playback->Play(result, 0) != MC_OK || playback->UseTrack(second, MC_PLAYBACK_TRACK_APPEND) != MC_OK) {
                return playbackFailure(__LINE__);
            }
            const auto preservedQueueSize = service.Count();

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
                service.Count() != preservedQueueSize) return playbackFailure(__LINE__);
            wake = query_test_wakeup_count();
            bool removed = false;
            api::ListPlaylists(context, *source, [&](mc_result status, auto entries, std::string_view) {
                removed = status == MC_OK && std::none_of(entries.begin(), entries.end(),
                    [&](const auto& entry) { return entry.id == playlistId; });
            });
            if (!query_test_wait_for_wakeup(wake + 1) || context.Dispatch() != MC_OK || !removed) return playbackFailure(__LINE__);

            mc_result otherThread = MC_OK;
            auto otherHandle = OpenFixturePlayback(service);
            mc_result otherOpen = MC_OK;
            std::thread caller([&] {
                mc_playback* offThread = nullptr;
                otherOpen = service.OpenHandle(&offThread);
                mc_playback_destroy(&offThread);
                otherThread = otherHandle.Control(MC_PLAYBACK_STOP);
            });
            caller.join();
            if (otherOpen != MC_ERROR_WRONG_THREAD || otherThread != MC_ERROR_WRONG_THREAD ||
                otherHandle.Control(MC_PLAYBACK_STOP) != MC_OK)
            {
                return playbackFailure(__LINE__);
            }

            /* A fresh service has no queued edit events from earlier scenarios. */
            runtime::MessageQueue stateMessages;
            const auto stateTransport = std::make_shared<TestTransport>();
            audio::PlaybackService stateService(stateMessages, local, stateTransport);
            stateMessages.Dispatch();
            auto statePlayback = OpenFixturePlayback(stateService);
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
            api::PlaybackEventSubscription subscriber(OpenFixturePlayback(stateService),
                [&](mc_playback_event event) {
                    deliveredOnOwner &= std::this_thread::get_id() == owner;
                    record(&scoped, event);
                });
            LiveEvents filtered;
            std::vector filter { MC_PLAYBACK_CHANGED_TRACK, MC_PLAYBACK_CHANGED_VOLUME };
            api::PlaybackEventSubscription filteredSubscriber(OpenFixturePlayback(stateService),
                [&](mc_playback_event event) { record(&filtered, event); }, filter);
            std::fill(filter.begin(), filter.end(), MC_PLAYBACK_CHANGED_MODE);

            unsigned destroyedCalls = 0;
            auto destroyed = std::make_unique<api::PlaybackEventSubscription>(OpenFixturePlayback(stateService),
                [&](mc_playback_event) { ++destroyedCalls; });

            if (statePlayback.Play(result, 1) != MC_OK) return playbackFailure(__LINE__);
            stateTransport->state = sdk::PlaybackState::Prepared;
            stateTransport->PlaybackEvent.Publish(sdk::PlaybackState::Prepared);
            stateTransport->stream = sdk::StreamState::Buffered;
            stateTransport->StreamEvent.Publish(sdk::StreamState::Buffered, stateTransport->Uri());
            stateTransport->VolumeChanged.Publish();
            stateTransport->VolumeChanged.Publish();
            stateTransport->TimeChanged.Publish(0);
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
            stateTransport->VolumeChanged.Publish();
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
            self = std::make_unique<api::PlaybackEventSubscription>(OpenFixturePlayback(stateService),
                [&](mc_playback_event) {
                    self.reset();
                    ++selfCalls;
                });
            stateTransport->VolumeChanged.Publish();
            stateTransport->TimeChanged.Publish(0);
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
            stateTransport->VolumeChanged.Publish();
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
            stateTransport->VolumeChanged.Publish();
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

            /* In-memory metadata can differ from the catalog row. */
            constexpr auto playbackTitle = "$Playback metadata";
            engineTrack->ClearValue(MC_TRACK_FIELD_TITLE);
            engineTrack->SetValue(MC_TRACK_FIELD_TITLE, playbackTitle);
            engineTrack->ClearValue(MC_TRACK_FIELD_ALBUM);
            engineTrack->ClearValue(MC_TRACK_FIELD_GENRE);
            engineTrack->SetValue(MC_TRACK_FIELD_GENRE, "Playback genre");
            stateTransport->state = sdk::PlaybackState::Stopped;
            stateTransport->stream = sdk::StreamState::Buffered;
            const auto metadata = statePlayback.CurrentTrackMetadata();
            if (!metadata || metadata->Id() != result.Id(1) ||
                metadata->Title() != playbackTitle || !metadata->Album().empty() || metadata->Genre() != "Playback genre")
            {
                return playbackFailure(__LINE__);
            }
            stateTransport->state = sdk::PlaybackState::Playing;

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
                statePlayback.CurrentTrack() || statePlayback.CurrentTrackMetadata() ||
                metadata->Title() != playbackTitle)
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

            /* Cancel an immediate start before the transport ever enters Playing. */
            stateTransport->PlaybackEvent.Publish(sdk::PlaybackState::Stopped);
            stateMessages.Dispatch();
            stateMessages.Dispatch();
            unsigned trackChanges = 0;
            const mc_playback_event trackEvent = MC_PLAYBACK_CHANGED_TRACK;
            if (mc_playback_set_changed_callback(statePlayback.Get(), [](void* data, mc_playback_event) {
                ++*static_cast<unsigned*>(data);
            }, &trackChanges, &trackEvent, 1) != MC_OK || statePlayback.Play(result, 1) != MC_OK)
            {
                return playbackFailure(__LINE__);
            }

            stateTransport->state = sdk::PlaybackState::Stopped;
            for (const auto stream : { sdk::StreamState::Buffering, sdk::StreamState::Buffered }) {
                stateTransport->stream = stream;
                stateTransport->StreamEvent.Publish(stream, stateTransport->Uri());
                stateMessages.Dispatch();
                stateMessages.Dispatch();
                if (trackChanges != 1 || !statePlayback.CurrentTrack()) {
                    return playbackFailure(__LINE__);
                }
            }

            if (statePlayback.Control(MC_PLAYBACK_STOP) != MC_OK) {
                return playbackFailure(__LINE__);
            }
            stateTransport->StreamEvent.Publish(sdk::StreamState::Stopped, stateTransport->Uri());
            stateMessages.Dispatch();
            stateMessages.Dispatch();
            if (trackChanges != 2 || statePlayback.CurrentTrack() || stateService.GetPlaying()) {
                return playbackFailure(__LINE__);
            }
            statePlayback.SetChangedCallback(nullptr);
        }
        return playback->Control(MC_PLAYBACK_STOP) == MC_ERROR_NOT_FOUND &&
            playback->SetChangedCallback(nullptr) == MC_ERROR_NOT_FOUND &&
            !playback->Snapshot() && !playback->Queue() && !playback->CurrentTrack();
    }
}
