#include "ApiHost.h"
#include <musikcore/musikcore.hpp>
#include <musikcore/sdk/version.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/db/Connection.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
    using Json = nlohmann::json;
    using Socket = websocketpp::server<websocketpp::config::asio>;

    /* prove the existing wire decoders preserve intersected selections and the
    track text filter; accepting command names alone cannot establish parity. */
    bool checkExistingQuerySemantics() {
        using namespace musik::core;
        namespace query = library::query;
        db::Connection db;
        if (db.Open(":memory:") != db::Okay) {
            return false;
        }
        const char* setup[] {
            "CREATE TABLE tracks(id INTEGER PRIMARY KEY, title TEXT, duration INTEGER, visible INTEGER, "
                "album_id INTEGER, visual_artist_id INTEGER, visual_genre_id INTEGER, disc INTEGER, track INTEGER)",
            "CREATE TABLE albums(id INTEGER PRIMARY KEY, name TEXT)",
            "CREATE TABLE artists(id INTEGER PRIMARY KEY, name TEXT)",
            "CREATE TABLE genres(id INTEGER PRIMARY KEY, name TEXT, sort_order INTEGER)",
            "INSERT INTO albums VALUES(88,'Album'),(99,'Other')",
            "INSERT INTO artists VALUES(77,'Artist'),(99,'Other')",
            "INSERT INTO genres VALUES(1,'Same name',1),(2,'Same name',2),(3,'Unknown',3)",
            "INSERT INTO tracks VALUES"
                "(42,'needle',1,1,88,77,1,0,1),(17,'needle',1,1,88,77,3,0,2),"
                "(20,'different',1,1,88,77,2,0,3),(21,'needle',1,1,88,99,2,0,4),"
                "(22,'needle',1,1,99,77,2,0,5),(23,'needle',1,0,88,77,2,0,6)"
        };
        for (const auto statement : setup) {
            if (db.Execute(statement) != db::Okay) {
                return false;
            }
        }
        const query::category::PredicateList selections { { "artist", 77 }, { "album", 88 } };
        query::CategoryTrackListQuery tracks(nullptr, selections, "needle", query::TrackSortType::Album);
        const auto decoded = query::CategoryTrackListQuery::DeserializeQuery(nullptr, tracks.SerializeQuery());
        if (!decoded->Run(db) || decoded->GetResult()->GetIds() != std::vector<int64_t> { 42, 17 }) {
            return false;
        }
        query::CategoryListQuery categories(query::CategoryListQuery::MatchType::Substring, "genre", selections);
        const auto decodedCategories = query::CategoryListQuery::DeserializeQuery(categories.SerializeQuery());
        return decodedCategories->Run(db) && decodedCategories->GetResult()->Count() == 3;
    }

    /* serve native facet replies on a real loopback connection. */
    class Server {
        public:
            /* start an isolated listener and worker without relying on a configured server. */
            Server() {
                this->socket.clear_access_channels(websocketpp::log::alevel::all);
                this->socket.init_asio(&this->io);
                this->socket.set_message_handler([this](auto connection, auto message) {
                    const Json request = Json::parse(message->get_payload());
                    Json reply { { "id", request.at("id") }, { "name", request.at("name") } };
                    if (request.at("name") == "authenticate") {
                        reply["options"]["environment"]["app_version"] =
                            std::to_string(MUSIKCUBE_VERSION_MAJOR) + "." +
                            std::to_string(MUSIKCUBE_VERSION_MINOR) + ".0";
                    }
                    else if (request.at("name") != "ping") {
                        const auto query = Json::parse(request.at("options").at("raw_query_data").get<std::string>());
                        ++this->commands;
                        if (query.at("name") != "FacetListQuery") {
                            this->valid = false;
                            this->socket.close(connection, websocketpp::close::status::going_away, "unsupported query");
                            return;
                        }
                        const auto& options = query.at("options");
                        this->valid = this->valid && options.at("trackSearch") == "needle" &&
                            options.at("regularPredicateList").dump() == R"([["artist",77],["album",88]])";
                        if (this->failFacet) {
                            reply["options"]["success"] = false;
                        }
                        else {
                            reply["options"]["raw_query_data"] = R"({"result":[{"id":1,"value":"Same name","type":"genre"},{"id":3,"value":"Unknown","type":"genre"}]})";
                        }
                        std::lock_guard<std::mutex> lock(this->mutex);
                        if (this->hold) {
                            this->hold = false;
                            this->held = [this, connection, reply] { this->Send(connection, reply); };
                            this->changed.notify_all();
                            return;
                        }
                    }
                    this->Send(connection, reply);
                });
                this->socket.listen(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
                asio::error_code error;
                this->port = this->socket.get_local_endpoint(error).port();
                if (error) {
                    throw std::runtime_error(error.message());
                }
                this->socket.start_accept();
                this->worker = std::thread([this] {
                    this->socket.run();
                });
            }

            /* join the socket worker before destroying held replies or listener state. */
            ~Server() {
                this->socket.stop();
                this->worker.join();
            }

            /* expose the assigned loopback port without reserving a fixed system port. */
            unsigned short Port() const { return this->port; }

            /* hold the next facet reply to establish a cancellation or reconnect race. */
            void Hold() {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->hold = true;
            }

            /* bound the wait for the reply gate so a missing query fails deterministically. */
            bool WaitHeld() {
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->changed.wait_for(lock, std::chrono::seconds(5), [this] {
                    return bool(this->held);
                });
            }

            /* resume the gated reply on the socket thread after the test changes request state. */
            void Release() {
                std::function<void()> send;
                {
                    std::lock_guard<std::mutex> lock(this->mutex);
                    send = std::move(this->held);
                }
                asio::post(this->io, std::move(send));
            }

            std::atomic<unsigned> commands { 0 };
            std::atomic<bool> valid { true };
            std::atomic<bool> failFacet { false };

        private:
            /* send through the real socket; a deliberately retired connection may reject the reply. */
            void Send(websocketpp::connection_hdl connection, const Json& reply) {
                websocketpp::lib::error_code error;
                this->socket.send(connection, reply.dump(), websocketpp::frame::opcode::text, error);
            }

            asio::io_context io;
            Socket socket;
            unsigned short port = 0;
            std::thread worker;
            std::mutex mutex;
            std::condition_variable changed;
            bool hold = false;
            std::function<void()> held;
    };

    /* retain callback data after its borrowed category entries expire. */
    struct Result {
            unsigned calls = 0;
            mc_result status = MC_ERROR_BUSY;
            std::vector<int64_t> ids;
    };

    /* count terminal callbacks and copy category identity for exact-once and routing assertions. */
    void MC_CALL completed(mc_result status, const mc_category_entry* entries, size_t count,
        const mc_error*, void* data) {
        auto& result = *static_cast<Result*>(data);
        ++result.calls;
        result.status = status;
        for (size_t i = 0; i < count; ++i) {
            result.ids.push_back(entries[i].id);
        }
    }
}

/* exercise native facet queries, cancellation, failure and reconnects through the public API. */
int main() {
    using namespace musik::core;
    using namespace musik::core::api;
    if (!checkExistingQuerySemantics()) {
        std::fprintf(stderr, "existing category wire semantics did not preserve text and both selections\n");
        return 1;
    }
    Server server;
    runtime::MessageQueue host;
    Context context(TestHostOptions(host));
    auto options = mc_remote_options MC_REMOTE_OPTIONS_DEFAULT;
    options.host = { "127.0.0.1", 9 };
    options.websocket_port = server.Port();
    options.max_pending_queries = 1;
    RemoteSession session(options);
    std::optional<mc_result> connected;
    /* pump owner-thread completions while the bounded host wait drives socket progress. */
    const auto wait = [&](auto ready) {
        return AwaitTestHost(host, [&] {
            return context.Dispatch() == MC_OK && ready();
        });
    };
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) {
        connected = status;
    });
    if (!wait([&] { return connected.has_value(); }) || connected != MC_OK) {
        return 1;
    }
    auto library = session.OpenLibrary();
    const mc_facet_selection selections[] { { MC_FACET_ARTIST, 77 }, { MC_FACET_ALBUM, 88 } };
    /* submit identical facet constraints through the public API for each server mode. */
    const auto submit = [&](Result& result, bool search, std::string_view text = "needle") {
        mc_request_id id = 0;
        const auto status = mc_library_list_facets_async(context.Get(), library.Get(), MC_FACET_GENRE,
            search ? mc_string_view { text.data(), text.size() } : mc_string_view { nullptr, 0 },
            selections, 2, completed, &result, &id);
        if (status != MC_OK) {
            throw std::runtime_error("facet admission failed");
        }
        return id;
    };
    /* a server without a capability flag must receive exactly one native query. */
    Result result;
    submit(result, true);
    if (!wait([&] { return result.calls == 1; }) || result.status != MC_OK ||
        result.ids != std::vector<int64_t> { 1, 3 } || !server.valid || server.commands != 1)
    {
        std::fprintf(stderr, "native facets failed: status=%d commands=%u\n", result.status, server.commands.load());
        return 1;
    }
    server.failFacet = true;
    Result failed;
    submit(failed, true);
    if (!wait([&] { return failed.calls == 1; }) || failed.status != MC_ERROR_IO || server.commands != 2) {
        return 1;
    }
    server.failFacet = false;
    server.Hold();
    Result canceled;
    const auto id = submit(canceled, true);
    if (!server.WaitHeld() || mc_request_cancel(context.Get(), id) != MC_OK ||
        !wait([&] { return canceled.calls == 1; }) || canceled.status != MC_ERROR_CANCELLED)
    {
        return 1;
    }
    server.Release();
    if (session.Disconnect() != MC_OK) {
        return 1;
    }
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) { connected = status; });
    if (!wait([&] { return connected.has_value(); }) || connected != MC_OK) {
        return 1;
    }
    library = session.OpenLibrary();
    /* retire an in-flight request with its connection before reconnecting. */
    server.Hold();
    Result disconnected;
    submit(disconnected, true);
    if (!server.WaitHeld() || session.Disconnect() != MC_OK ||
        !wait([&] { return disconnected.calls == 1; }) || disconnected.status != MC_ERROR_IO)
    {
        return 1;
    }
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) { connected = status; });
    if (!wait([&] { return connected.has_value(); }) || connected != MC_OK) {
        return 1;
    }
    library = session.OpenLibrary();
    server.Release();
    Result reconnected;
    submit(reconnected, true);
    if (!wait([&] { return reconnected.calls == 1; }) || reconnected.status != MC_OK ||
        reconnected.ids != std::vector<int64_t> { 1, 3 } || canceled.calls != 1 ||
        disconnected.calls != 1 || server.commands != 5 || !server.valid)
    {
        return 1;
    }
    return context.Shutdown() == MC_OK ? 0 : 1;
}
