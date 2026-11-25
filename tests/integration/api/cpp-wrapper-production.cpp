#include "cpp-wrapper-cases.h"
#include <musikcore/musikcore.hpp>
#include "../../support/CoreRuntime.h"
#include <common/runtime/Message.h>
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <set>

namespace wrapper_tests {
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

    /* Verify typed remote query serialization preserves ordering/grouping and rejects malformed payloads. */
    bool checkRemoteQueryPayloads() {
        using namespace musik::core;
        using namespace musik::core::library;
        using namespace musik::core::library::query;

        const auto local = LibraryFactory::Instance().DefaultLocalLibrary();
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
        const auto selected = LibraryFactory::Instance().DefaultLocalLibrary();
        const auto source = OpenFixtureLibrary();
        if (!source) return false;

        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
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
            if (selected->EnqueueAndWait(categories, kWaitIndefinite) < 0 || !categories->GetResult()->size()) return false;
            api::OpenCategory(context, *source, key, categories->GetResult()->at(0).id,
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
        const auto library = OpenFixtureLibrary();
        if (!library) return false;
        mc_result completed = MC_OK;
        TrackResult tracks;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = query_test_wakeup;
        Context context(options);
        const auto wakes = query_test_wakeup_count();

        /* A retained handle keeps its backend after the current selection clears. */
        context.SetLibrary(*library);
        if (mc_context_set_library(context.Get(), nullptr) != MC_OK || context.GetLibrary()) {
            return false;
        }

        OpenTrackSearch(context, *library, TrackSearch{ "Fixture Artist" },
            [&](mc_result status, TrackResult result, std::string_view) {
                if (status == MC_OK) tracks = std::move(result);
            });
        if (!query_test_wait_for_wakeup(wakes + 1) || context.Dispatch() != MC_OK || tracks.Size() != 2) return false;

        /* Closing the origin rejects reads even while its database file is present. */
        LibraryFactory::Instance().DefaultLocalLibrary()->Close();
        OpenTrackSearch(context, *library, TrackSearch{ "Fixture Artist" },
            [&](mc_result status, TrackResult, std::string_view) { completed = status; });
        return query_test_wait_for_wakeup(wakes + 2) && context.Dispatch() == MC_OK && completed == MC_ERROR_IO;
    }
}
