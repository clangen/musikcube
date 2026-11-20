#include "TestTransport.h"
#include <musikcore/ffi/consumer/playback-binding.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/musikcore.hpp>
#include <musikcore/net/WebSocketClient.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/sdk/version.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Preferences.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
    using musik::core::net::WebSocketClient;
    using musik::core::library::query::SearchTrackListQuery;
    using musik::core::library::query::TrackSortType;

    /* Owns its loopback listener and worker. Neither copyable nor movable. */
    class Server {
    public:
        explicit Server(bool catalog = false) {
            this->socket.clear_access_channels(websocketpp::log::alevel::all);
            this->socket.init_asio(&this->io);
            this->socket.set_message_handler([this, catalog](auto connection, auto message) {
                const nlohmann::json request = nlohmann::json::parse(message->get_payload());
                auto reply = nlohmann::json{ { "id", request.at("id") }, { "name", request.at("name") } };
                if (request.at("name") == "authenticate") {
                    reply["options"]["environment"]["app_version"] =
                        std::to_string(MUSIKCUBE_VERSION_MAJOR) + "." +
                        std::to_string(MUSIKCUBE_VERSION_MINOR) + ".0";
                    this->Send(connection, reply);
                }
                else if (catalog) {
                    const auto query = nlohmann::json::parse(request.at("options").at("raw_query_data").get<std::string>());
                    if (query.at("name") == SearchTrackListQuery::kQueryName) {
                        reply["options"]["raw_query_data"] = R"({"result":{"headers":[0],"durations":{"0":10},"trackList":[42,17]}})";
                    }
                    else {
                        reply["options"]["raw_query_data"] =
                            R"({"result":{"42":{"id":42,"title":"Origin","replayGain":{}}}})";
                    }
                    if (++this->requests >= 3) {
                        std::lock_guard<std::mutex> lock(this->heldMutex);
                        this->held = [this, connection, reply](bool disconnect) {
                            if (disconnect) this->socket.close(connection, websocketpp::close::status::going_away, "");
                            else this->Send(connection, reply);
                        };
                        this->heldChanged.notify_all();
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
            if (error) throw std::runtime_error(error.message());
            this->socket.start_accept();
            this->worker = std::thread([this] { this->socket.run(); });
        }

        ~Server() {
            this->socket.stop();
            this->worker.join();
        }

        unsigned short Port() const { return this->port; }

        bool ReleaseHeld(bool disconnect) {
            std::unique_lock<std::mutex> lock(this->heldMutex);
            if (!this->heldChanged.wait_for(lock, std::chrono::seconds(5), [this] { return bool(this->held); })) return false;
            auto action = std::move(this->held);
            this->held = {};
            asio::post(this->io, [action, disconnect] { action(disconnect); });
            return true;
        }

    private:
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
        void OnClientInvalidPassword(Client*) override {}
        void OnClientStateChanged(Client*, State state, State) override {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->connected = state == State::Connected;
            this->changed.notify_all();
        }
        void OnClientQuerySucceeded(Client*, const std::string&, Client::Query) override {
            std::lock_guard<std::mutex> lock(this->mutex);
            ++this->successes;
            this->changed.notify_all();
        }
        void OnClientQueryFailed(Client*, const std::string& id, Client::Query, QueryError error) override {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->failures.emplace_back(id, error);
            this->changed.notify_all();
        }

        bool WaitForConnection() {
            std::unique_lock<std::mutex> lock(this->mutex);
            return this->changed.wait_for(lock, std::chrono::seconds(5), [this] { return this->connected; });
        }

        bool WaitForFailures(size_t count) {
            std::unique_lock<std::mutex> lock(this->mutex);
            return this->changed.wait_for(lock, std::chrono::seconds(5),
                [this, count] { return this->failures.size() >= count || this->successes; });
        }

        bool Matches(const std::string& malformed, const std::string& unanswered) const {
            std::lock_guard<std::mutex> lock(this->mutex);
            return !this->successes && this->failures == decltype(this->failures){
                { malformed, QueryError::ParseFailed }, { unanswered, QueryError::Disconnected }
            };
        }

    private:
        mutable std::mutex mutex;
        std::condition_variable changed;
        bool connected = false;
        unsigned successes = 0;
        std::vector<std::pair<std::string, QueryError>> failures;
    };
    bool checkRemoteAbi() {
        using namespace musik::core;
        using namespace musik::core::api;

        Server server(true);
        const auto settings = Preferences::ForComponent(prefs::components::Settings);
        settings->SetString(prefs::keys::RemoteLibraryHostname, "127.0.0.1");
        settings->SetInt(prefs::keys::RemoteLibraryWssPort, server.Port());
        settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
        settings->SetBool(prefs::keys::SaveSessionOnExit, false);
        struct Shutdown {
            ~Shutdown() { LibraryFactory::Shutdown(); }
        } shutdown;

        const auto master = std::make_shared<library::MasterLibrary>();
        auto& selection = *master;
        runtime::MessageQueue messageQueue;
        audio::PlaybackService service(messageQueue, master, std::make_shared<TestTransport>());
        auto playback = musik::core::audio::CreatePlaybackHandle(service);
        const auto wait = [](auto ready) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            do {
                if (ready()) return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        };

        std::shared_ptr<const Library> origin;
        if (!wait([&] { return bool(origin = selection.OpenApiLibrary()); })) return false;

        TrackResult tracks;
        TrackPage page;
        unsigned calls = 0;
        mc_result status = MC_ERROR_INTERNAL;
        Context context;
        const auto dispatch = [&](unsigned expected) {
            return wait([&] { return context.Dispatch() == MC_OK && calls == expected; });
        };

        OpenTrackSearch(context, *origin, TrackSearch{ "Origin" },
            [&](mc_result result, TrackResult found, std::string_view) {
                status = result;
                tracks = std::move(found);
                ++calls;
            });
        if (!dispatch(1) || status != MC_OK || tracks.Size() != 2 || tracks.Id(0) != 42 ||
            tracks.Id(1) != 17 || !tracks.StartsAlbum(0) || tracks.AlbumDurationSeconds(0) != 10) return false;

        const TrackRange range{ 0, 2, { MC_TRACK_FIELD_TITLE } };
        const auto receive = [&](mc_result result, TrackPage found, std::string_view) {
            status = result;
            page = std::move(found);
            ++calls;
        };
        ReadTracks(context, tracks, range, receive);
        if (!dispatch(2) || status != MC_OK || page.Size() != 2 || page.Track(0).Title() != "Origin" ||
            page.Id(1) != 17 || page.Track(1).Value(MC_TRACK_FIELD_TITLE).data()) return false;

        const auto reference = tracks.Track(0);
        if (playback.UseTrack(reference, MC_PLAYBACK_TRACK_APPEND) != MC_OK ||
            service.Count() != 1) return false;

        /* Switch before releasing a cancelled remote read. The next remote request stays on its origin. */
        const auto pending = ReadTracks(context, tracks, range, receive);
        settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
        selection.LoadDefaultLibrary();
        if (playback.UseTrack(reference, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND) return false;
        audio::DiscardUnavailablePlaybackQueue(service);
        if (service.Count() != 0) return false;
        const auto local = selection.OpenApiLibrary();
        if (!local) return false;
        QueryTracks(context, *local, TrackQuery{ "" }, receive);
        if (!dispatch(3) || status != MC_OK) return false;

        if (context.Cancel(pending) != MC_OK || !dispatch(4) || status != MC_ERROR_CANCELLED ||
            !server.ReleaseHeld(false)) return false;
        ReadTrack(context, reference, { MC_TRACK_FIELD_TITLE }, receive);
        if (!server.ReleaseHeld(true) || !dispatch(5) || status != MC_ERROR_IO) return false;

        /* Reconnecting creates a new generation even when the endpoint stays the same. */
        const auto remote = std::dynamic_pointer_cast<library::RemoteLibrary>(LibraryFactory::Instance().DefaultRemoteLibrary());
        remote->ReloadConnectionFromPreferences();
        settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));
        selection.LoadDefaultLibrary();
        std::shared_ptr<const Library> reconnected;
        if (!wait([&] { return bool(reconnected = selection.OpenApiLibrary()); })) return false;
        ReadTrack(context, reference, { MC_TRACK_FIELD_TITLE }, receive);
        if (!dispatch(6) || status != MC_ERROR_IO) return false;

        OpenTrackSearch(context, *reconnected, TrackSearch{ "Origin" },
            [&](mc_result result, TrackResult found, std::string_view) {
                status = result;
                tracks = std::move(found);
                ++calls;
            });
        if (!server.ReleaseHeld(false) || !dispatch(7) || status != MC_OK || tracks.Size() != 2) return false;

        /* The new generation returns the same ID. Only its new reference can edit the queue. */
        const auto current = tracks.Track(0);
        if (current.Id() != reference.Id() ||
            playback.UseTrack(current, MC_PLAYBACK_TRACK_APPEND) != MC_OK ||
            playback.UseTrack(reference, MC_PLAYBACK_TRACK_APPEND) != MC_ERROR_NOT_FOUND ||
            service.Count() != 1 || service.GetTrackList()->GetId(0) != current.Id()) return false;

        /* Cancel and drain each ABI request while the server holds every reply. */
        constexpr unsigned burstSize = 65;
        for (unsigned i = 0; i < burstSize; ++i) {
            const auto request = OpenTrackSearch(context, *reconnected, TrackSearch{ "Origin" },
                [&](mc_result result, TrackResult, std::string_view) {
                    status = result;
                    ++calls;
                });
            const auto before = calls;
            const auto cancelled = context.Cancel(request);
            if (!dispatch(before + 1)) return false;
            if (status == MC_ERROR_BUSY) {
                return i > 0 && cancelled == MC_ERROR_NOT_FOUND && server.ReleaseHeld(true);
            }
            if (status != MC_ERROR_CANCELLED || cancelled != MC_OK) return false;
        }
        return false;
    }
}

int main() {
    try {
        Server server;
        Listener listener;
        WebSocketClient client(nullptr, &listener);
        client.Connect("127.0.0.1", server.Port(), "", false);
        if (!listener.WaitForConnection()) return 1;

        const auto enqueue = [&] {
            return client.EnqueueQuery(std::make_shared<SearchTrackListQuery>(nullptr,
                SearchTrackListQuery::MatchType::Substring, "", TrackSortType::Album));
        };
        const auto malformed = enqueue();
        if (malformed.empty() || !listener.WaitForFailures(1)) return 1;
        const auto unanswered = enqueue();
        if (unanswered.empty() || !listener.WaitForFailures(2)) return 1;

        client.Disconnect();
        if (!listener.Matches(malformed, unanswered)) return 1;
        if (checkRemoteAbi()) return 0;
        std::fputs("Remote library ABI contract failed\n", stderr);
        return 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Remote query transport failed: %s\n", error.what());
        return 1;
    }
}
