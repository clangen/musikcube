#include "TestTransport.h"
#include <musikcore/ffi/consumer/playback-binding.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/AllCategoriesQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/DeletePlaylistQuery.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/musikcore.hpp>
#include <musikcore/net/WebSocketClient.h>
#include <musikcore/plugin/Plugins.h>
#include "CoreRuntime.h"
#include "ApiHost.h"
#include "../../support/profile.hpp"
#include <musikcore/library/QueryRegistry.h>
#include <musikcore/db/Connection.h>
#include <musikcore/db/Statement.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/sdk/version.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/Common.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstring>

namespace {
    /* Print the failed contract's source line while preserving the fixture's boolean assertion flow. */
    bool failure(int line) {
        std::fprintf(stderr, "Remote API contract failed at line %d\n", line);
        return false;
    }
    constexpr int64_t kPlaylistId = 7;
    constexpr int kAppendPlaylistOperation = 4;
    constexpr char kHeldSearch[] = "Held remote search";
    using musik::core::net::WebSocketClient;
    using musik::core::library::query::SearchTrackListQuery;
    using musik::core::library::query::TrackSortType;

    /* Owns its loopback listener and worker. Neither copyable nor movable. */
    class Server {
        public:
            /* Bind a dynamic loopback port and select real typed-query forwarding or controlled mock replies. */
            explicit Server(bool catalog = false, musik::core::ILibraryPtr local = {}) {
                this->socket.clear_access_channels(websocketpp::log::alevel::all);
                this->socket.init_asio(&this->io);
                this->socket.set_message_handler([this, catalog, local](auto connection, auto message) {
                    const nlohmann::json request = nlohmann::json::parse(message->get_payload());
                    auto reply = nlohmann::json { { "id", request.at("id") }, { "name", request.at("name") } };
                    if (request.at("name") == "authenticate") {
                        reply["options"]["environment"]["app_version"] =
                            std::to_string(MUSIKCUBE_VERSION_MAJOR) + "." +
                            std::to_string(MUSIKCUBE_VERSION_MINOR) + ".0";
                        this->Send(connection, reply);
                    }
                    else if (request.at("name") == "ping") {
                        this->Send(connection, reply);
                    }
                    else if (local) {
                        /* Forward real typed queries instead of inventing a second
                           catalog implementation; parity failures then isolate the
                           adapter/serialization boundary rather than SQL differences. */
                        const auto raw = request.at("options").at("raw_query_data").get<std::string>();
                        const auto query = nlohmann::json::parse(raw);
                        const auto command = musik::core::library::QueryRegistry::CreateLocalQueryFor(
                            query.at("name").get<std::string>(), raw, local);
                        if (!command || local->EnqueueAndWait(command) < 0 ||
                            command->GetStatus() != musik::core::db::IQuery::Finished)
                        {
                            this->socket.close(connection, websocketpp::close::status::going_away, "Query failed");
                            return;
                        }
                        reply["options"]["raw_query_data"] = command->SerializeResult();
                        if (query.at("name") == SearchTrackListQuery::kQueryName &&
                            query.at("options").at("filter") == kHeldSearch)
                        {
                            this->HoldReply(connection, reply);
                            return;
                        }
                        this->Send(connection, reply);
                    }
                    else if (catalog) {
                        /* Legacy responses deliberately omit projectedPage to prove
                           the new client still reads an older server's map format. */
                        const auto query = nlohmann::json::parse(request.at("options").at("raw_query_data").get<std::string>());
                        if (query.at("name") == SearchTrackListQuery::kQueryName) {
                            reply["options"]["raw_query_data"] = R"({"result":{"headers":[0],"durations":{"0":10},"trackList":[42,17]}})";
                            if (query.at("options").at("filter") == "Invalid IDs") {
                                reply["options"]["raw_query_data"] = R"({"result":{"headers":[],"durations":{},"trackList":[0]}})";
                            }
                        }
                        else if (query.at("name") == musik::core::library::query::GetPlaylistQuery::kQueryName) {
                            reply["options"]["raw_query_data"] = R"({"result":{"headers":[],"trackList":[42,17,42]}})";
                        }
                        else if (query.at("name") == musik::core::library::query::CategoryTrackListQuery::kQueryName ||
                            query.at("name") == musik::core::library::query::DirectoryTrackListQuery::kQueryName)
                        {
                            reply["options"]["raw_query_data"] =
                                R"({"result":{"headers":[0,2],"durations":{"0":10,"2":4},"trackList":[42,17,42]}})";
                        }
                        else if (query.at("name") == musik::core::library::query::CategoryListQuery::kQueryName) {
                            reply["options"]["raw_query_data"] = nlohmann::json {
                                { "result", { { { "id", kPlaylistId }, { "value", "Loopback Playlist" }, { "type", "playlists" } } } }
                            }.dump();
                        }
                        else if (query.at("name") == musik::core::library::query::AllCategoriesQuery::kQueryName) {
                            reply["options"]["raw_query_data"] =
                                R"({"result":[{"id":0,"value":"album","type":"category"},{"id":0,"value":"genre","type":"category"}]})";
                        }
                        else if (query.at("name") == musik::core::library::query::SavePlaylistQuery::kQueryName) {
                            const auto& options = query.at("options");
                            const bool valid = options.at("op") == kAppendPlaylistOperation &&
                                options.at("playlistId") == kPlaylistId &&
                                options.at("tracks") == nlohmann::json::array({ 42, 17, 42 });
                            reply["options"]["raw_query_data"] = nlohmann::json {
                                { "result", valid }, { "playlistId", kPlaylistId }
                            }.dump();
                        }
                        else if (query.at("name") == musik::core::library::query::DeletePlaylistQuery::kQueryName) {
                            reply["options"]["raw_query_data"] = nlohmann::json {
                                { "result", query.at("options").at("playlistId") == kPlaylistId }
                            }.dump();
                        }
                        else {
                            reply["options"]["raw_query_data"] =
                                R"({"result":{"42":{"id":42,"title":"Origin","replayGain":{}}}})";
                        }
                        if (++this->requests >= 3) {
                            /* Keep replies pending until the test selects cancellation,
                               a reconnect, or shutdown; timing sleeps cannot establish
                               those races reliably. */
                            this->HoldReply(connection, reply);
                            return;
                        }
                        this->Send(connection, reply);
                    }
                    else if (++this->requests == 1) {
                        /* Duplicate malformed replies still settle the query once. */
                        reply["options"]["success"] = true;
                        this->Send(connection, reply);
                        this->Send(connection, reply);
                    }
                    else {
                        /* Close while a second request has no reply. */
                        this->socket.close(connection, websocketpp::close::status::going_away, "");
                    }
                });
                this->socket.listen(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
                asio::error_code error;
                this->port = this->socket.get_local_endpoint(error).port();
                if (error) {
                    throw std::runtime_error(error.message());
                }
                this->socket.start_accept();
                this->worker = std::thread([this] { this->socket.run(); });
            }

            /* Stop and join the listener worker before its socket and held-reply state are destroyed. */
            ~Server() {
                this->socket.stop();
                this->worker.join();
            }

            /* Expose the dynamically allocated listener port for isolated client preferences. */
            unsigned short Port() const { return this->port; }

            bool WaitForHeldReply() {
                std::unique_lock<std::mutex> lock(this->heldMutex);
                return this->heldChanged.wait_for(lock, std::chrono::seconds(5), [this] {
                    return bool(this->held);
                });
            }

            /* Wait for a withheld response, then send it or disconnect on the server's own event loop. */
            bool ReleaseHeld(bool disconnect) {
                std::unique_lock<std::mutex> lock(this->heldMutex);
                if (!this->heldChanged.wait_for(lock, std::chrono::seconds(5), [this] { return bool(this->held); })) {
                    return false;
                }
                auto action = std::move(this->held);
                this->held = {};
                asio::post(this->io, [action, disconnect] { action(disconnect); });
                return true;
            }

        private:
            void HoldReply(websocketpp::connection_hdl connection, const nlohmann::json& reply) {
                std::lock_guard<std::mutex> lock(this->heldMutex);
                this->held = [this, connection, reply](bool disconnect) {
                    if (disconnect) {
                        this->socket.close(connection, websocketpp::close::status::going_away, "");
                    }
                    else {
                        websocketpp::lib::error_code error;
                        this->socket.send(connection, reply.dump(), websocketpp::frame::opcode::text, error);
                    }
                };
                this->heldChanged.notify_all();
            }

            /* Serialize one protocol response onto the accepted loopback websocket. */
            void Send(websocketpp::connection_hdl connection, const nlohmann::json& reply) {
                this->socket.send(connection, reply.dump(), websocketpp::frame::opcode::text);
            }

            asio::io_context io;
            websocketpp::server<websocketpp::config::asio> socket;
            std::thread worker;
            unsigned short port = 0;
            unsigned requests = 0;
            std::mutex heldMutex;
            std::condition_variable heldChanged;
            std::function<void(bool)> held;
    };

    /* Owns observations shared by the test and client threads. Neither copyable nor movable. */
    class Listener final : public WebSocketClient::Listener {
        public:
            /* Ignore authentication failures in this fixture; its deterministic server accepts authentication. */
            void OnClientInvalidPassword(Client*) override {}
            /* Publish connection readiness under the listener mutex and wake deadline-based observers. */
            void OnClientStateChanged(Client*, State state, State) override {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->connected = state == State::Connected;
                this->changed.notify_all();
            }
            /* Record unexpected successes so malformed/disconnected response tests cannot pass silently. */
            void OnClientQuerySucceeded(Client*, const std::string&, Client::Query) override {
                std::lock_guard<std::mutex> lock(this->mutex);
                ++this->successes;
                this->changed.notify_all();
            }
            /* Retain failed request IDs and reasons in delivery order for exactly-once assertions. */
            void OnClientQueryFailed(Client*, const std::string& id, Client::Query, QueryError error) override {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->failures.emplace_back(id, error);
                this->changed.notify_all();
            }

            /* Wait until authentication completes instead of relying on socket-startup timing. */
            bool WaitForConnection() {
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->changed.wait_for(lock, std::chrono::seconds(5), [this] { return this->connected; });
            }

            /* Wait for the expected failures, but stop early on an unexpected success. */
            bool WaitForFailures(size_t count) {
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->changed.wait_for(lock, std::chrono::seconds(5),
                    [this, count] { return this->failures.size() >= count || this->successes; });
            }

            /* Verify one parse failure followed by one disconnect failure, with no successful or duplicate
               delivery. */
            bool Matches(const std::string& malformed, const std::string& unanswered) const {
                std::lock_guard<std::mutex> lock(this->mutex);
                return !this->successes && this->failures == decltype(this->failures) { { malformed, QueryError::ParseFailed }, { unanswered, QueryError::Disconnected } };
            }

        private:
            mutable std::mutex mutex;
            std::condition_variable changed;
            bool connected = false;
            unsigned successes = 0;
            std::vector<std::pair<std::string, QueryError>> failures;
    };
    /* Hold real socket replies across selection/reconnect/teardown so old generations cannot edit the
       current queue or complete requests twice. */
    bool checkRemoteAbi() {
        using namespace musik::core;
        using namespace musik::core::api;

        Server server(true);
        const auto settings = musik::core::Preferences::ForComponent(prefs::components::Settings);
        settings->SetString(prefs::keys::RemoteLibraryHostname, "127.0.0.1");
        settings->SetInt(prefs::keys::RemoteLibraryWssPort, server.Port());
        settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
        settings->SetBool(prefs::keys::SaveSessionOnExit, false);
        struct SelectionNotice {
            Context* context = nullptr;
            bool local = false;
        } notice;
        std::vector<mc_indexer_event> indexEvents;
        runtime::MessageQueue host;
        auto options = TestHostOptions(host);
        options.flags = MC_CONTEXT_MANAGED_RUNTIME;
        Context context(options);
        notice.context = &context;
        const auto wait = [&](auto ready) {
            return AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && ready(); });
        };

        /* Drain the lazy database-upgrade scan before scheduling the test scan. */
        LibraryFactory::Instance().DefaultLocalLibrary()->Indexer()->Shutdown();
        if (!wait([&] { return context.GetLibrary().has_value(); }) ||
            context.GetLibraryStatus().kind != MC_LIBRARY_REMOTE ||
            context.GetLibraryStatus().state != MC_LIBRARY_CONNECTED ||
            context.GetLibraryStatus().endpoint.empty())
        {
            return failure(__LINE__);
        }

        /* The local indexer remains addressable with a remote catalog selected. */
        Indexer indexer(context);
        const auto originalPaths = indexer.Paths();
        const auto testPath = GetDataDirectory() + "/indexer-contract-path";
        if (indexer.AddPath(testPath) != MC_OK) {
            return failure(__LINE__);
        }
        const auto addedPaths = indexer.Paths();
        if (std::find(addedPaths.begin(), addedPaths.end(), testPath) == addedPaths.end() ||
            indexer.RemovePath(testPath) != MC_OK || indexer.Paths() != originalPaths ||
            indexer.SetCallback([](void* data, mc_indexer_event event, int) {
                static_cast<std::vector<mc_indexer_event>*>(data)->push_back(event);
            }, &indexEvents) != MC_OK ||
            indexer.Schedule(MC_INDEXER_SCAN_LOCAL) != MC_OK ||
            !wait([&] {
                return context.Dispatch() == MC_OK && !indexEvents.empty() &&
                    indexEvents.back() == MC_INDEXER_FINISHED;
            }) || indexEvents.front() != MC_INDEXER_STARTED || indexer.SetCallback(nullptr) != MC_OK)
        {
            return failure(__LINE__);
        }

        if (context.SetLibraryChangedCallback([](void* data, bool selectionChanged) {
            auto& notice = *static_cast<SelectionNotice*>(data);
            if (selectionChanged && notice.context->GetLibraryStatus().kind == MC_LIBRARY_LOCAL) {
                notice.local = notice.context->GetLibrary().has_value();
            }
        }, &notice) != MC_OK || mc_context_set_library(context.Get(), nullptr) != MC_ERROR_UNSUPPORTED)
        {
            return failure(__LINE__);
        }

        TrackResult tracks;
        TrackPage page;
        unsigned calls = 0;
        mc_result status = MC_ERROR_INTERNAL;
        auto playback = context.GetPlayback();
        const auto queueSize = [&] {
            const auto queue = playback.Queue();
            return queue ? queue->Size() : SIZE_MAX;
        };
        const auto queueId = [&](size_t row) {
            const auto queue = playback.Queue();
            size_t count = 0;
            const auto* ids = queue ? queue->Ids(count) : nullptr;
            return ids && row < count ? ids[row] : int64_t {};
        };
        const auto dispatch = [&](unsigned expected) {
            return wait([&] { return context.Dispatch() == MC_OK && calls == expected; });
        };

        OpenTrackSearch(context, TrackSearch { "Origin" },
            [&](mc_result result, TrackResult found, std::string_view) {
                status = result;
                tracks = std::move(found);
                ++calls;
            });
        if (!dispatch(1) || status != MC_OK || tracks.Size() != 2 || tracks.Id(0) != 42 ||
            tracks.Id(1) != 17 || !tracks.StartsAlbum(0) || tracks.AlbumDurationSeconds(0) != 10)
        {
            return failure(__LINE__);
        }

        const TrackRange range { 0, 2, { MC_TRACK_FIELD_TITLE } };
        const auto receive = [&](mc_result result, TrackPage found, std::string_view error) {
            status = (result == MC_OK) == error.empty() ? result : MC_ERROR_INTERNAL;
            page = std::move(found);
            ++calls;
        };
        ReadTracks(context, tracks, range, receive);
        if (!dispatch(2) || status != MC_OK || page.Size() != 2 || page.Track(0).Title() != "Origin" ||
            page.Id(1) != 17 || page.Track(1).Value(MC_TRACK_FIELD_TITLE).data())
        {
            return failure(__LINE__);
        }

        const auto reference = tracks.Track(0);
        if (playback.UseTrack(reference, MC_PLAYBACK_TRACK_APPEND) != MC_OK ||
            queueSize() != 1)
        {
            return failure(__LINE__);
        }

        /* Search rows and playback must share the selected backend. */
        if (playback.ReplaceQueue(tracks) != MC_OK || queueSize() != tracks.Size()) {
            return failure(__LINE__);
        }
        if (playback.HotSwapQueue(tracks, 0) != MC_OK || queueSize() != tracks.Size()) {
            return failure(__LINE__);
        }
        if (playback.InsertResult(tracks, MC_PLAYBACK_TRACK_APPEND) != MC_OK) {
            return failure(__LINE__);
        }
        const auto queue = context.GetPlayback().Queue();
        size_t queued = 0;
        const auto ids = queue ? queue->Ids(queued) : nullptr;
        if (!ids || queued != tracks.Size() * 2 ||
            !std::equal(ids, ids + tracks.Size(), ids + tracks.Size()))
        {
            return failure(__LINE__);
        }

        /* Switch before releasing a cancelled remote read. The next remote request stays on its origin. */
        const auto pending = ReadTracks(context, tracks, range, receive);
        if (context.SelectLibrary(MC_LIBRARY_LOCAL) != MC_OK || notice.local ||
            context.Dispatch() != MC_OK || !notice.local)
        {
            return failure(__LINE__);
        }
        if (playback.UseTrack(reference, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND) {
            return failure(__LINE__);
        }
        if (playback.HotSwapQueue(tracks, 0) != MC_ERROR_NOT_FOUND) {
            return failure(__LINE__);
        }
        if (playback.InsertResult(tracks, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND) {
            return failure(__LINE__);
        }
        if (queueSize() != 0) {
            return failure(__LINE__);
        }
        if (context.GetLibraryStatus().kind != MC_LIBRARY_LOCAL || !context.GetLibrary()) {
            return failure(__LINE__);
        }
        QueryTracks(context, TrackQuery { "" }, receive);
        if (!dispatch(3) || status != MC_OK) {
            std::fprintf(stderr, "Local handoff: status=%d calls=%u\n", status, calls);
            return failure(__LINE__);
        }

        if (context.Cancel(pending) != MC_OK || !dispatch(4) || status != MC_ERROR_CANCELLED ||
            !server.ReleaseHeld(false))
        {
            return failure(__LINE__);
        }
        ReadTrack(context, reference, { MC_TRACK_FIELD_TITLE }, receive);
        if (!server.ReleaseHeld(true) || !dispatch(5) || status != MC_ERROR_IO) {
            return failure(__LINE__);
        }

        /* Reconnecting creates a new generation even when the endpoint stays the same. */
        const auto remote = std::dynamic_pointer_cast<library::RemoteLibrary>(LibraryFactory::Instance().DefaultRemoteLibrary());
        remote->Connect();
        if (context.SelectLibrary(MC_LIBRARY_REMOTE) != MC_OK) {
            return failure(__LINE__);
        }
        std::optional<Library> reconnected;
        if (!wait([&] { return bool(reconnected = context.GetLibrary()); })) {
            return failure(__LINE__);
        }
        ReadTrack(context, reference, { MC_TRACK_FIELD_TITLE }, receive);
        if (!dispatch(6) || status != MC_ERROR_IO) {
            return failure(__LINE__);
        }

        OpenPlaylist(context, *reconnected, 1,
            [&](mc_result result, TrackResult found, std::string_view) {
                status = result;
                tracks = std::move(found);
                ++calls;
            });
        if (!server.ReleaseHeld(false) || !dispatch(7) || status != MC_OK || tracks.Size() != 3 ||
            tracks.Id(0) != 42 || tracks.Id(1) != 17 || tracks.Id(2) != 42 ||
            tracks.AlbumDurationSeconds(0) != 0 ||
            playback.ReplaceQueue(tracks) != MC_OK ||
            queueSize() != 3 || queueId(2) != 42)
        {
            return failure(__LINE__);
        }
        if (playback.ClearQueue() != MC_OK) {
            return failure(__LINE__);
        }

        /* The new generation returns the same ID. Only its new reference can edit the queue. */
        const auto current = tracks.Track(0);
        if (current.Id() != reference.Id() ||
            playback.UseTrack(current, MC_PLAYBACK_TRACK_APPEND) != MC_OK ||
            playback.UseTrack(reference, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND ||
            queueSize() != 1 || queueId(0) != current.Id())
        {
            return failure(__LINE__);
        }

        bool listed = false;
        ListPlaylists(context, *reconnected, [&](mc_result result, auto playlists, std::string_view) {
            listed = result == MC_OK && playlists.size() == 1 &&
                playlists.front().id == kPlaylistId && playlists.front().name == "Loopback Playlist";
            ++calls;
        });
        if (!server.ReleaseHeld(false) || !dispatch(8) || !listed) {
            return failure(__LINE__);
        }
        const auto written = [&](mc_result result, int64_t id, std::string_view) {
            status = id == kPlaylistId ? result : MC_ERROR_NOT_FOUND;
            ++calls;
        };
        AppendPlaylist(context, *reconnected, kPlaylistId, { 42, 17, 42 }, written);
        if (!server.ReleaseHeld(false) || !dispatch(9) || status != MC_OK) {
            return failure(__LINE__);
        }
        DeletePlaylist(context, *reconnected, kPlaylistId, written);
        if (!server.ReleaseHeld(false) || !dispatch(10) || status != MC_OK) {
            return failure(__LINE__);
        }

        const auto receiveSelection = [&](mc_result result, TrackResult selection, std::string_view) {
            status = result;
            tracks = std::move(selection);
            ++calls;
        };
        OpenCategory(context, *reconnected, MC_TRACK_FIELD_ALBUM, 1, MC_TRACK_SEARCH_ALBUM, receiveSelection);
        if (!server.ReleaseHeld(false) || !dispatch(11) || status != MC_OK || tracks.Size() != 3 ||
            !tracks.StartsAlbum(2) || tracks.AlbumDurationSeconds(2) != 4)
        {
            return failure(__LINE__);
        }
        const auto album = std::move(tracks);
        OpenDirectory(context, *reconnected, "/remote/music", receiveSelection);
        if (!server.ReleaseHeld(false) || !dispatch(12) || status != MC_OK || tracks.Size() != 3 ||
            tracks.Id(0) != 42 || tracks.Id(1) != 17 || tracks.Id(2) != 42 || !tracks.StartsAlbum(0))
        {
            return failure(__LINE__);
        }

        OpenTrackSearch(context, *reconnected, TrackSearch { "Invalid IDs" }, receiveSelection);
        if (!server.ReleaseHeld(false) || !dispatch(13) || status != MC_ERROR_IO || tracks) {
            return failure(__LINE__);
        }

        /* Album menu replies and previously loaded albums cannot edit the queue after reconnect. */
        OpenCategory(context, *reconnected, MC_TRACK_FIELD_ALBUM, 1, MC_TRACK_SEARCH_ALBUM, receiveSelection);
        if (!server.WaitForHeldReply()) {
            return failure(__LINE__);
        }
        const auto generation = remote->WebSocketClient().ConnectionGeneration();
        remote->Disconnect();
        if (!wait([&] { return !context.GetLibrary(); }) ||
            context.GetLibraryStatus().state == MC_LIBRARY_CONNECTED)
        {
            return failure(__LINE__);
        }
        remote->Connect();
        if (!wait([&] {
            const auto connected = remote->WebSocketClient().ConnectionGeneration();
            return connected && connected != generation && (reconnected = context.GetLibrary());
        }) || !server.ReleaseHeld(false) || !dispatch(14) || status != MC_ERROR_IO || tracks)
        {
            return failure(__LINE__);
        }
        const auto preservedSize = queueSize();
        if (playback.Play(album, 0) != MC_ERROR_NOT_FOUND ||
            playback.InsertResult(album, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND ||
            queueSize() != preservedSize)
        {
            return failure(__LINE__);
        }

        std::vector<std::string> fields;
        ListCategoryFields(context, [&](mc_result result, std::vector<std::string> values, std::string_view) {
            if (result == MC_OK) {
                fields = std::move(values);
            }
        });
        if (!server.ReleaseHeld(false) || !wait([&] { return context.Dispatch() == MC_OK && !fields.empty(); }) ||
            fields != std::vector<std::string> { MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_GENRE })
        {
            return failure(__LINE__);
        }

        /* Keep one accepted read pending through shutdown; cancel the rest while
           replies are held, until the transport reports its bounded admission. */
        constexpr unsigned burstSize = 65;
        for (unsigned i = 0; i < burstSize; ++i) {
            const auto request = OpenTrackSearch(context, *reconnected, TrackSearch { "Origin" },
                [&](mc_result result, TrackResult, std::string_view) {
                    status = result;
                    ++calls;
                });
            if (i == 0) {
                continue;
            }
            const auto before = calls;
            const auto cancelled = context.Cancel(request);
            if (!dispatch(before + 1)) {
                return failure(__LINE__);
            }
            if (status == MC_ERROR_BUSY) {
                if (cancelled != MC_ERROR_NOT_FOUND || context.Shutdown() != MC_OK ||
                    calls != before + 2 || status != MC_ERROR_CANCELLED || !server.ReleaseHeld(true))
                {
                    return failure(__LINE__);
                }
                const auto settled = calls;
                return context.Dispatch() == MC_OK && calls == settled;
            }
            if (status != MC_ERROR_CANCELLED || cancelled != MC_OK) {
                return failure(__LINE__);
            }
        }
        return failure(__LINE__);
    }
    /* Use the same typed backend on both sides of a real socket to expose serialization losses
       independently of catalog differences. */
    bool checkBackendParity(const char* database) {
        using namespace musik::core;
        using namespace musik::core::api;
        CoreRuntime runtime;
        auto& host = runtime.Queue();
        const auto settings = musik::core::Preferences::ForComponent(prefs::components::Settings);
        settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
        const auto local = OpenFixtureLibrary();
        if (!local) {
            return failure(__LINE__);
        }
        Server server(false, LibraryFactory::Instance().DefaultLocalLibrary());
        mc_remote_state connectionState = MC_REMOTE_DISCONNECTED;
        Context context(TestHostOptions(host));
        mc_remote_options options = MC_REMOTE_OPTIONS_DEFAULT;
        constexpr std::string_view loopback = "127.0.0.1";
        options.host = { loopback.data(), loopback.size() };
        options.websocket_port = server.Port();
        RemoteSession session(options);

        const auto connect = [&] {
            connectionState = MC_REMOTE_CONNECTING;
            const auto request = session.Connect(context,
                [&](mc_result result, mc_remote_state state, std::string_view) {
                    connectionState = result == MC_OK ? state : MC_REMOTE_DISCONNECTED;
                });
            const bool completed = request && AwaitTestHost(host, [&] {
                return context.Dispatch() == MC_OK && connectionState != MC_REMOTE_CONNECTING;
            });
            return completed && connectionState == MC_REMOTE_CONNECTED && session.State() == MC_REMOTE_CONNECTED;
        };
        if (!connect()) {
            return failure(__LINE__);
        }
        const auto remote = std::make_shared<const Library>(session.OpenLibrary());

        for (const auto& library : { local, remote }) {
            context.SetLibrary(*library);
            auto matches = std::make_shared<std::optional<bool>>();
            ListCategories(context, MC_TRACK_FIELD_ARTIST, "Fixture", MC_TRACK_SEARCH_SUBSTRING,
                [matches](mc_result status, std::vector<CategoryEntry> categories, std::string_view) {
                    *matches = status == MC_OK && categories.size() == 1 &&
                        categories.front().value == "Fixture Artist";
                });
            if (!AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && matches->has_value(); }) ||
                !matches->value())
            {
                return failure(__LINE__);
            }
        }
        context.ClearLibrary();

        TrackResult localTracks, remoteTracks;
        TrackPage localPage, remotePage;
        mc_result status = MC_ERROR_INTERNAL;
        unsigned calls = 0;
        int64_t playlist = 0;
        const auto dispatch = [&](unsigned expected) {
            return AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && calls == expected; }) && status == MC_OK;
        };
        const auto resultMatches = [](const TrackResult& a, const TrackResult& b) {
            if (a.Size() != b.Size()) {
                return failure(__LINE__);
            }
            for (size_t i = 0; i < a.Size(); ++i) {
                if (a.Id(i) != b.Id(i) || a.StartsAlbum(i) != b.StartsAlbum(i) ||
                    a.AlbumDurationSeconds(i) != b.AlbumDurationSeconds(i))
                {
                    return failure(__LINE__);
                }
            }
            return true;
        };
        const auto receiveResult = [&](TrackResult& output) {
            return [&](mc_result result, TrackResult value, std::string_view) {
                status = result;
                output = std::move(value);
                ++calls;
            };
        };
        const auto receivePage = [&](TrackPage& output) {
            return [&](mc_result result, TrackPage value, std::string_view) {
                status = result;
                output = std::move(value);
                ++calls;
            };
        };
        using Clock = std::chrono::steady_clock;
        const auto micros = [](Clock::time_point start) {
            return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        };
        std::vector<double> localSearchTimes, remoteSearchTimes, localReadTimes, remoteReadTimes;
        for (unsigned sample = 0; sample < 7; ++sample) {
            for (bool overRemote : { false, true }) {
                const auto& target = overRemote ? remote : local;
                auto& output = overRemote ? remoteTracks : localTracks;
                ProfilePhase searchProfile(sample == 6 ? (overRemote ? "remote_search" : "local_search") : nullptr);
                auto start = Clock::now();
                const auto expected = calls + 1;
                OpenTrackSearch(context, *target, {}, receiveResult(output));
                if (!dispatch(expected)) {
                    return failure(__LINE__);
                }
                (overRemote ? remoteSearchTimes : localSearchTimes).push_back(micros(start));
                searchProfile.Finish();
                const TrackRange fields { 0, 1000, { MC_TRACK_FIELD_ID, MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION, MC_TRACK_FIELD_TRACK, MC_TRACK_FIELD_RATING } };
                ProfilePhase readProfile(sample == 6 ? (overRemote ? "remote_page" : "local_page") : nullptr);
                start = Clock::now();
                const auto readExpected = calls + 1;
                ReadTracks(context, output, fields, receivePage(overRemote ? remotePage : localPage));
                if (!dispatch(readExpected)) {
                    return failure(__LINE__);
                }
                (overRemote ? remoteReadTimes : localReadTimes).push_back(micros(start));
                readProfile.Finish();
            }
            if (!localTracks.Size() || !resultMatches(localTracks, remoteTracks) || localPage.Size() != remotePage.Size()) {
                return failure(__LINE__);
            }
            for (size_t row = 0; row < localPage.Size(); ++row) {
                if (localPage.Id(row) != remotePage.Id(row)) {
                    return failure(__LINE__);
                }
                for (size_t field = 0; field < localPage.FieldCount(); ++field) {
                    if (localPage.Value(row, field) != remotePage.Value(row, field)) {
                        return failure(__LINE__);
                    }
                }
            }
        }

        /* The same ordered IDs cross both backends, including duplicates and a missing row. */
        const int64_t reordered[] = { localTracks.Id(0), localTracks.Id(0), INT64_MAX };
        for (bool overRemote : { false, true }) {
            auto result = MakeTrackResult(overRemote ? *remote : *local, reordered, 3);
            const auto expected = calls + 1;
            ReadTracks(context, result, { 0, 3, { MC_TRACK_FIELD_TITLE } }, receivePage(overRemote ? remotePage : localPage));
            if (!dispatch(expected)) {
                return failure(__LINE__);
            }
        }
        if (localPage.Size() != 3 || remotePage.Size() != 3 || localPage.Id(2) != INT64_MAX ||
            remotePage.Id(2) != INT64_MAX || localPage.Value(2, 0).data() || remotePage.Value(2, 0).data() ||
            localPage.Value(0, 0) != remotePage.Value(0, 0) || localPage.Value(1, 0) != remotePage.Value(1, 0))
        {
            return failure(__LINE__);
        }

        for (bool overRemote : { false, true }) {
            auto missing = MakeTrackResult(overRemote ? *remote : *local, reordered + 2, 1).Track(0);
            const auto expected = calls + 1;
            ReadTrack(context, missing, { MC_TRACK_FIELD_TITLE }, receivePage(overRemote ? remotePage : localPage));
            if (!AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && calls == expected; }) ||
                status != MC_ERROR_NOT_FOUND || (overRemote ? bool(remotePage) : bool(localPage)))
            {
                return failure(__LINE__);
            }
        }
        const auto beforeUnsupported = calls;
        const auto emptyRemote = MakeTrackResult(*remote, nullptr, 0);
        try {
            ReadTracks(context, emptyRemote, { 0, 1, { MC_TRACK_FIELD_GENRE } }, receivePage(remotePage));
            return failure(__LINE__);
        }
        catch (const Error& error) {
            if (error.Result() != MC_ERROR_UNSUPPORTED) {
                return failure(__LINE__);
            }
        }
        if (context.Dispatch() != MC_OK || calls != beforeUnsupported) {
            return failure(__LINE__);
        }
        const auto emptyExpected = calls + 1;
        ReadTracks(context, emptyRemote, {}, receivePage(remotePage));
        if (!dispatch(emptyExpected) || !remotePage || remotePage.Size() || remotePage.FieldCount() != 4) {
            return failure(__LINE__);
        }

        /* Preserve SQL NULL, explicit empty text, Unicode, and hidden membership
           through the same typed projection on both sides of the socket. */
        const int64_t edgeIds[] = { localTracks.Id(0), localTracks.Id(1) };
        const std::string longTitle = std::string("音楽 🎵 ") + std::string(65536, 'x');
        /* Seed metadata edge cases without passing test-only types into core. */
        db::Connection fixture;
        if (fixture.Open(database) != db::Okay) {
            return failure(__LINE__);
        }
        const auto mutate = [&](int64_t id, const std::string& title, bool visible) {
            db::Statement update("UPDATE tracks SET title=?, rating=NULL, visible=? WHERE id=?", fixture);
            update.BindText(0, title);
            update.BindInt32(1, visible ? 1 : 0);
            update.BindInt64(2, id);
            return update.Step() == db::Done && fixture.LastModifiedRowCount() == 1;
        };
        const auto readEdges = [&] {
            for (bool overRemote : { false, true }) {
                auto result = MakeTrackResult(overRemote ? *remote : *local, edgeIds, 2);
                const auto expected = calls + 1;
                ReadTracks(context, result, { 0, 2, { MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_RATING } },
                    receivePage(overRemote ? remotePage : localPage));
                if (!dispatch(expected)) {
                    return false;
                }
            }
            if (localPage.Size() != 2 || remotePage.Size() != 2) {
                return false;
            }
            for (size_t row = 0; row < 2; ++row) {
                for (size_t field = 0; field < 2; ++field) {
                    if (localPage.ValueCount(row, field) != remotePage.ValueCount(row, field) ||
                        bool(localPage.Value(row, field).data()) != bool(remotePage.Value(row, field).data()) ||
                        localPage.Value(row, field) != remotePage.Value(row, field))
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        if (!mutate(edgeIds[0], "", true) || !mutate(edgeIds[1], longTitle, true) || !readEdges()) {
            return failure(__LINE__);
        }
        if (!remotePage.Value(0, 0).data() || !remotePage.Value(0, 0).empty() ||
            remotePage.Value(0, 1).data() || remotePage.Value(1, 0) != longTitle)
        {
            return failure(__LINE__);
        }
        if (!mutate(edgeIds[0], "", false) || !readEdges() || remotePage.Value(0, 0).data() ||
            remotePage.Id(0) != edgeIds[0] || remotePage.Value(1, 0) != longTitle)
        {
            return failure(__LINE__);
        }
        if (!mutate(edgeIds[0], "", true)) {
            return failure(__LINE__);
        }

        /* A write through either backend is visible through the other. */
        for (bool overRemote : { false, true }) {
            const uint32_t rating = overRemote ? MC_TRACK_RATING_MAX : 0;
            const auto expectedRating = calls + 1;
            SetTrackRating(context, (overRemote ? remoteTracks : localTracks).Track(0), rating,
                [&](mc_result result, std::string_view) {
                    status = result;
                    ++calls;
                });
            if (!dispatch(expectedRating)) {
                return failure(__LINE__);
            }
            const auto expectedRead = calls + 1;
            ReadTrack(context, (overRemote ? localTracks : remoteTracks).Track(0),
                { MC_TRACK_FIELD_RATING }, receivePage(localPage));
            if (!dispatch(expectedRead) || localPage.Track(0).Rating() != rating) {
                return failure(__LINE__);
            }
        }

        const std::vector<int64_t> ids(10000, localTracks.Id(0));
        {
            ProfilePhase profile("save_playlist_10000");
            const auto expected = calls + 1;
            SavePlaylist(context, *local, 0, "API Baseline Playlist", ids,
                [&](mc_result result, int64_t id, std::string_view) { status = result; playlist = id; ++calls; });
            if (!dispatch(expected) || playlist <= 0) {
                return failure(__LINE__);
            }
        }
        std::vector<double> localPlaylistTimes, remotePlaylistTimes, queueTimes;
        /* Playback still binds to the app's selected backend. */
        settings->SetString(prefs::keys::RemoteLibraryHostname, loopback.data());
        settings->SetInt(prefs::keys::RemoteLibraryWssPort, server.Port());
        settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
        const auto appRemote = std::static_pointer_cast<library::RemoteLibrary>(
            LibraryFactory::Instance().DefaultRemoteLibrary());
        appRemote->Connect();
        std::shared_ptr<const Library> playbackLibrary;
        if (!AwaitTestHost(host, [&] { return bool(playbackLibrary = OpenFixtureLibrary(ILibrary::Type::Remote)); })) {
            return failure(__LINE__);
        }
        TrackResult queueTracks;
        const auto queueExpected = calls + 1;
        OpenPlaylist(context, *playbackLibrary, playlist, receiveResult(queueTracks));
        if (!dispatch(queueExpected)) {
            return failure(__LINE__);
        }
        const auto transport = std::make_shared<TestTransport>();
        transport->state = sdk::PlaybackState::Stopped;
        runtime::MessageQueue playbackMessages;
        audio::PlaybackService service(playbackMessages, appRemote, transport);
        auto playback = audio::CreatePlaybackHandle(service);
        for (unsigned sample = 0; sample < 7; ++sample) {
            for (bool overRemote : { false, true }) {
                ProfilePhase profile(sample == 6 ? (overRemote ? "remote_playlist_10000" : "local_playlist_10000") : nullptr);
                const auto start = Clock::now();
                const auto expected = calls + 1;
                OpenPlaylist(context, overRemote ? *remote : *local, playlist, receiveResult(overRemote ? remoteTracks : localTracks));
                if (!dispatch(expected)) {
                    return failure(__LINE__);
                }
                (overRemote ? remotePlaylistTimes : localPlaylistTimes).push_back(micros(start));
            }
            if (localTracks.Size() != ids.size() || !resultMatches(localTracks, remoteTracks)) {
                return failure(__LINE__);
            }
            ProfilePhase profile(sample == 6 ? "replace_queue_10000" : nullptr);
            const auto start = Clock::now();
            if (playback.ReplaceQueue(queueTracks) != MC_OK || service.Count() != ids.size()) {
                return failure(__LINE__);
            }
            queueTimes.push_back(micros(start));
        }
        const auto expected = calls + 1;
        DeletePlaylist(context, *remote, playlist,
            [&](mc_result result, int64_t, std::string_view) { status = result; ++calls; });
        if (!dispatch(expected)) {
            return failure(__LINE__);
        }
        const auto report = [](const char* name, std::vector<double> values) {
            std::sort(values.begin(), values.end());
            std::printf("%s: median_us=%.3f p95_us=%.3f samples=%zu\n", name,
                values[values.size() / 2], values.back(), values.size());
        };
        report("local_search", localSearchTimes);
        report("remote_search", remoteSearchTimes);
        report("local_page", localReadTimes);
        report("remote_page", remoteReadTimes);
        report("local_playlist_10000", localPlaylistTimes);
        report("remote_playlist_10000", remotePlaylistTimes);
        report("replace_queue_10000", queueTimes);

        /* Hold a reply across disconnect and reconnect before opening the replacement library. */
        TrackResult lateTracks;
        const auto lateExpected = calls + 1;
        OpenTrackSearch(context, *remote, { kHeldSearch }, receiveResult(lateTracks));
        if (!server.WaitForHeldReply() || session.Disconnect() != MC_OK || !connect() ||
            !server.ReleaseHeld(false) || calls != lateExpected || status != MC_ERROR_IO || lateTracks)
        {
            return failure(__LINE__);
        }

        /* A retained result must stay bound to the connection that produced it. */
        const auto staleExpected = calls + 1;
        ReadTracks(context, remoteTracks, { 0, 1, { MC_TRACK_FIELD_TITLE } }, receivePage(remotePage));
        if (!AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && calls == staleExpected; }) ||
            status != MC_ERROR_IO || remotePage)
        {
            return failure(__LINE__);
        }

        const auto staleWriteExpected = calls + 1;
        SetTrackRating(context, remoteTracks.Track(0), 0,
            [&](mc_result result, std::string_view) {
                status = result;
                ++calls;
            });
        if (!AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && calls == staleWriteExpected; }) ||
            status != MC_ERROR_IO)
        {
            return failure(__LINE__);
        }

        const auto newLibrary = session.OpenLibrary();
        const auto newExpected = calls + 1;
        OpenTrackSearch(context, newLibrary, {}, receiveResult(remoteTracks));
        if (!dispatch(newExpected) || !remoteTracks) {
            return failure(__LINE__);
        }
        return context.Shutdown() == MC_OK;
    }
    bool checkManagedRemote() {
        using namespace musik::core;
        using namespace musik::core::api;

        Server server(true);
        auto settings = musik::core::Preferences::ForComponent(prefs::components::Settings);
        settings->SetString(prefs::keys::RemoteLibraryHostname, "127.0.0.1");
        settings->SetInt(prefs::keys::RemoteLibraryWssPort, server.Port());
        settings->SetBool(prefs::keys::RemoteLibraryViewed, true);

        TrackResult tracks;
        TrackPage page;
        std::optional<mc_result> completed;
        runtime::MessageQueue host;
        auto options = TestHostOptions(host);
        options.flags = MC_CONTEXT_MANAGED_RUNTIME;
        Context context(options);
        const auto wait = [&](auto ready) {
            return AwaitTestHost(host, [&] { return context.Dispatch() == MC_OK && ready(); });
        };
        const auto remoteReady = [&] {
            const auto status = context.GetLibraryStatus();
            return status.kind == MC_LIBRARY_REMOTE && status.state == MC_LIBRARY_CONNECTED;
        };

        if (context.GetLibraryStatus().kind != MC_LIBRARY_LOCAL ||
            context.SelectLibrary(MC_LIBRARY_REMOTE) != MC_OK || !wait(remoteReady))
        {
            return failure(__LINE__);
        }
        OpenTrackSearch(context, {}, [&](mc_result result, TrackResult selection, std::string_view) {
            completed = result;
            tracks = std::move(selection);
        });
        if (!wait([&] { return completed.has_value(); }) || completed != MC_OK || tracks.Size() != 2) {
            return failure(__LINE__);
        }

        auto origin = tracks.Track(0);
        auto playback = context.GetPlayback();
        if (playback.UseTrack(origin, MC_PLAYBACK_TRACK_APPEND) != MC_OK) {
            return failure(__LINE__);
        }
        const auto remoteQueue = playback.Queue();
        if (!remoteQueue || remoteQueue->Size() != 1 || context.SelectLibrary(MC_LIBRARY_LOCAL) != MC_OK ||
            context.GetLibraryStatus().kind != MC_LIBRARY_LOCAL ||
            playback.UseTrack(origin, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND ||
            playback.Remove(*remoteQueue, 0) != MC_ERROR_NOT_FOUND)
        {
            return failure(__LINE__);
        }
        const auto localQueue = playback.Queue();
        if (!localQueue || localQueue->Size() != 0)
        {
            return failure(__LINE__);
        }

        const auto readOrigin = [&] {
            completed.reset();
            ReadTrack(context, origin, { MC_TRACK_FIELD_TITLE },
                [&](mc_result result, TrackPage found, std::string_view) {
                    completed = result;
                    page = std::move(found);
                });
            return wait([&] { return completed.has_value(); });
        };
        if (!readOrigin() || completed != MC_OK || page.Size() != 1 || page.Track(0).Title() != "Origin") {
            return failure(__LINE__);
        }

        /* A new connection to the same address must still invalidate old references. */
        if (context.SelectLibrary(MC_LIBRARY_REMOTE) != MC_OK || context.ReconnectRemoteLibrary() != MC_OK) {
            return failure(__LINE__);
        }
        if (!wait(remoteReady)) {
            return failure(__LINE__);
        }
        if (!readOrigin() || completed != MC_ERROR_IO) {
            std::fprintf(stderr, "Retained remote read completed with %d\n", completed.value_or(MC_ERROR_BUSY));
            return failure(__LINE__);
        }
        if (playback.UseTrack(origin, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND) {
            return failure(__LINE__);
        }
        return context.Shutdown() == MC_OK;
    }
}

/* Run parity, managed runtime, or malformed-response scenarios in an isolated runtime. */
int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::strcmp(argv[1], "--parity") == 0) {
            return argc == 3 && checkBackendParity(argv[2]) ? 0 : 1;
        }
        if (argc > 1 && std::strcmp(argv[1], "--managed") == 0) {
            return checkManagedRemote() ? 0 : 1;
        }
        Server server;
        Listener listener;
        WebSocketClient client(nullptr, &listener);
        client.Connect("127.0.0.1", server.Port(), "", false);
        if (!listener.WaitForConnection()) {
            return 1;
        }

        const auto enqueue = [&] {
            return client.EnqueueQuery(std::make_shared<SearchTrackListQuery>(nullptr,
                SearchTrackListQuery::MatchType::Substring, "", TrackSortType::Album));
        };
        const auto malformed = enqueue();
        if (malformed.empty() || !listener.WaitForFailures(1)) {
            return 1;
        }
        const auto unanswered = enqueue();
        if (unanswered.empty() || !listener.WaitForFailures(2)) {
            return 1;
        }

        client.Disconnect();
        if (!listener.Matches(malformed, unanswered)) {
            return 1;
        }
        if (checkRemoteAbi()) {
            return 0;
        }
        std::fputs("Remote library ABI contract failed\n", stderr);
        return 1;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Remote query transport failed: %s\n", error.what());
        return 1;
    }
}
