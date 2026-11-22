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

    /* serve controlled legacy and advertised facet replies on a real loopback
    connection so routing, request counts and connection boundaries are observable. */
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
                        if (this->advertise) {
                            reply["options"]["environment"]["contextual_facets"] = true;
                        }
                    }
                    else if (request.at("name") != "ping") {
                        const auto query = Json::parse(request.at("options").at("raw_query_data").get<std::string>());
                        ++this->commands;
                        if (query.at("name") == "FacetListQuery") {
                            if (!this->advertise) {
                                this->valid = false;
                            }
                            if (!this->advertise || this->failFacet) {
                                reply["options"]["success"] = false;
                            }
                            else {
                                this->valid = this->valid && query.at("options").at("trackSearch") == "needle";
                                reply["options"]["raw_query_data"] = R"({"result":[{"id":1,"value":"Same name","type":"genre"},{"id":3,"value":"Unknown","type":"genre"}]})";
                            }
                        }
                        else if (query.at("name") == "CategoryListQuery") {
                            const auto& predicates = query.at("options").at("regularPredicateList");
                            this->valid = this->valid && predicates.dump() == R"([["artist",77],["album",88]])";
                            reply["options"]["raw_query_data"] = R"({"result":[{"id":1,"value":"Same name","type":"genre"},{"id":2,"value":"Same name","type":"genre"},{"id":3,"value":"Unknown","type":"genre"}]})";
                        }
                        else if (query.at("name") == "CategoryTrackListQuery") {
                            ++this->probes;
                            const auto& options = query.at("options");
                            const auto predicates = options.at("regularPredicateList").dump();
                            this->valid = this->valid && (options.at("filter") == "needle" || options.at("filter") == "bulk") &&
                                predicates == R"([["artist",77],["album",88]])";
                            reply["options"]["raw_query_data"] =
                                R"({"result":{"headers":[],"durations":{},"trackList":[42,17,42]}})";
                            if (options.at("filter") == "bulk") {
                                auto ids = Json::array();
                                for (int64_t id = 1; id <= 1001; ++id) {
                                    ids.push_back(id);
                                }
                                reply["options"]["raw_query_data"] = Json {
                                    { "result", { { "headers", Json::array() }, { "durations", Json::object() }, { "trackList", ids } } }
                                }.dump();
                            }
                            std::lock_guard<std::mutex> lock(this->mutex);
                            if (this->hold) {
                                this->hold = false;
                                this->held = [this, connection, reply] {
                                    this->Send(connection, reply);
                                };
                                this->changed.notify_all();
                                return;
                            }
                        }
                        else if (query.at("name") == "TrackMetadataBatchQuery") {
                            /* legacy full-track maps expose actual visual category IDs;
                            identical display names must not merge distinct categories. */
                            const auto& ids = query.at("options").at("trackIds");
                            this->valid = this->valid && ids.size() <= 500;
                            Json tracks = Json::object();
                            for (const auto& value : ids) {
                                const auto id = value.get<int64_t>();
                                tracks[std::to_string(id)] = {
                                    { "id", id }, { "visual_genre_id", id == 17 ? "3" : "1" }, { "replayGain", Json::object() }
                                };
                            }
                            reply["options"]["raw_query_data"] = Json { { "result", tracks } }.dump();
                        }
                        else {
                            this->valid = false;
                            this->socket.close(connection, websocketpp::close::status::going_away, "unsupported query");
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

            /* hold the next matching-track reply to establish a cancellation or reconnect race. */
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
            std::atomic<unsigned> probes { 0 };
            std::atomic<bool> valid { true };
            std::atomic<bool> advertise { false };
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

/* exercise legacy composition, bounded metadata reads and advertised native routing
through the public asynchronous API, with controlled cancellation and reconnects. */
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
    /* distinguish same-name categories by ID and retain the matching unknown category. */
    Result result;
    submit(result, true);
    if (!wait([&] { return result.calls == 1; }) || result.status != MC_OK ||
        result.ids != std::vector<int64_t> { 1, 3 } || !server.valid || server.commands != 3)
    {
        std::fprintf(stderr, "contextual facets failed: status=%d commands=%u probes=%u\n",
            result.status, server.commands.load(), server.probes.load());
        return 1;
    }
    /* empty text must need only one constrained category query. */
    Result unfiltered;
    submit(unfiltered, false);
    if (!wait([&] { return unfiltered.calls == 1; }) || unfiltered.status != MC_OK ||
        unfiltered.ids.size() != 3 || server.commands != 4)
    {
        std::fprintf(stderr, "unfiltered facets failed: status=%d commands=%u\n", unfiltered.status, server.commands.load());
        return 1;
    }
    /* more than two metadata batches must remain bounded to 500 IDs per wire command. */
    Result bulk;
    submit(bulk, true, "bulk");
    if (!wait([&] { return bulk.calls == 1; }) || bulk.status != MC_OK ||
        bulk.ids != std::vector<int64_t> { 1, 3 } || server.commands != 9 || !server.valid)
    {
        std::fprintf(stderr, "bounded facet batches failed: status=%d commands=%u\n", bulk.status, server.commands.load());
        return 1;
    }
    /* cancel while the track reply is held; releasing it must not start metadata reads. */
    server.Hold();
    Result canceled;
    const auto canceledId = submit(canceled, true);
    if (!server.WaitHeld() || mc_request_cancel(context.Get(), canceledId) != MC_OK ||
        !wait([&] { return canceled.calls == 1; }) || canceled.status != MC_ERROR_CANCELLED)
    {
        std::fprintf(stderr, "facet cancellation failed: status=%d calls=%u\n", canceled.status, canceled.calls);
        return 1;
    }
    server.Release();
    Result barrier;
    /* cancellation releases the API slot before the held wire reply arrives.
    retry only admission failures, bounded by a deadline, until that slot drains. */
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        barrier = {};
        submit(barrier, false);
        if (!wait([&] { return barrier.calls == 1; })) {
            return 1;
        }
    } while (barrier.status == MC_ERROR_BUSY && std::chrono::steady_clock::now() < deadline);
    if (barrier.status != MC_OK || canceled.calls != 1 || server.probes != 3 || server.commands != 12) {
        std::fprintf(stderr, "post-cancel facets failed: status=%d calls=%u probes=%u\n",
            barrier.status, barrier.calls, server.probes.load());
        return 1;
    }

    /* a reconnect must not send any successor probe into the new generation. */
    server.Hold();
    Result disconnected;
    submit(disconnected, true);
    if (!server.WaitHeld() || session.Disconnect() != MC_OK ||
        !wait([&] { return disconnected.calls == 1; }) || disconnected.status != MC_ERROR_IO)
    {
        std::fprintf(stderr, "facet disconnect failed: status=%d calls=%u\n",
            disconnected.status, disconnected.calls);
        return 1;
    }
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) {
        connected = status;
    });
    if (!wait([&] { return connected.has_value(); }) || connected != MC_OK) {
        return 1;
    }
    library = session.OpenLibrary();
    server.Release();
    Result reconnected;
    submit(reconnected, false);
    if (!wait([&] { return reconnected.calls == 1; }) || reconnected.status != MC_OK ||
        disconnected.calls != 1 || server.probes != 4 || server.commands != 15 || !server.valid)
    {
        return 1;
    }

    /* advertised support keeps current servers on one native query. a genuine
    query failure remains an error, never a capability retry or composition. */
    if (session.Disconnect() != MC_OK) {
        return 1;
    }
    server.advertise = true;
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) { connected = status; });
    if (!wait([&] { return connected.has_value(); }) || connected != MC_OK) {
        return 1;
    }
    library = session.OpenLibrary();
    Result native;
    submit(native, true);
    if (!wait([&] { return native.calls == 1; }) || native.status != MC_OK ||
        native.ids != std::vector<int64_t> { 1, 3 } || server.commands != 16)
    {
        std::fprintf(stderr, "advertised native facets did not use one command: status=%d commands=%u\n",
            native.status, server.commands.load());
        return 1;
    }
    server.failFacet = true;
    Result failed;
    submit(failed, true);
    if (!wait([&] { return failed.calls == 1; }) || failed.status != MC_ERROR_IO || server.commands != 17) {
        return 1;
    }
    /* a new handshake without the capability must retire the previous native routing decision. */
    if (session.Disconnect() != MC_OK) {
        return 1;
    }
    server.advertise = false;
    server.failFacet = false;
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) { connected = status; });
    if (!wait([&] { return connected.has_value(); }) || connected != MC_OK) {
        return 1;
    }
    library = session.OpenLibrary();
    Result reset;
    submit(reset, true);
    if (!wait([&] { return reset.calls == 1; }) || reset.status != MC_OK ||
        reset.ids != std::vector<int64_t> { 1, 3 } || server.commands != 20 || !server.valid)
    {
        return 1;
    }
    return context.Shutdown() == MC_OK ? 0 : 1;
}
