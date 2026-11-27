#include "ApiHost.h"
#include <musikcore/musikcore.hpp>
#include <common/version.h>
#include <musikcore/library/query/AlbumListQuery.h>
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
            "ALTER TABLE tracks ADD COLUMN rating INTEGER DEFAULT 0",
            "CREATE TABLE meta_keys(id INTEGER PRIMARY KEY, name TEXT)",
            "CREATE TABLE meta_values(id INTEGER PRIMARY KEY, meta_key_id INTEGER, content TEXT)",
            "CREATE TABLE track_meta(track_id INTEGER, meta_value_id INTEGER)",
            "INSERT INTO meta_keys VALUES(1,'year')", "INSERT INTO meta_values VALUES(5,1,'1995'),(6,1,'2000')",
            "INSERT INTO track_meta VALUES(42,5),(17,5),(20,6)",
            "CREATE VIEW extended_metadata AS SELECT tracks.id, 0 AS source_id, meta_values.id AS meta_value_id, "
            "meta_keys.name AS key, meta_values.content AS value FROM tracks JOIN track_meta ON "
            "tracks.id=track_meta.track_id JOIN meta_values ON meta_values.id=track_meta.meta_value_id JOIN meta_keys "
            "ON meta_keys.id=meta_values.meta_key_id",
            "INSERT INTO albums VALUES(88,'Album'),(99,'Other')",
            "INSERT INTO artists VALUES(77,'Artist'),(99,'Other')",
            "INSERT INTO genres VALUES(1,'Same name',1),(2,'Same name',2),(3,'Unknown',3)",
            "INSERT INTO tracks(id,title,duration,visible,album_id,visual_artist_id,visual_genre_id,disc,track) VALUES"
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
        if (!decodedCategories->Run(db) || decodedCategories->GetResult()->size() != 3) {
            return false;
        }
        if (db.Execute("INSERT INTO genres VALUES(4,'   ',2)") != db::Okay ||
            db.Execute(
                "INSERT INTO tracks(id,title,duration,visible,album_id,visual_artist_id,visual_genre_id,disc,track) "
                "VALUES(24,'needle',1,1,88,77,4,0,7)") != db::Okay)
        {
            return false;
        }
        query::CategoryListQuery page(query::CategoryListQuery::MatchType::Substring, "genre", selections);
        page.SetTrackFilter("needle");
        page.SetPage(4, 0, 1);
        if (!page.Run(db) || page.GetTotal() != 3 || page.GetAnchorIndex() != 1 || page.GetResult()->size() != 1 ||
            page.GetResult()->at(0).id != 1)
        {
            return false;
        }
        page.SetPage(4, 1, 1);
        if (!page.Run(db) || page.GetTotal() != 3 || page.GetAnchorIndex() != 1 || page.GetResult()->size() != 1 ||
            page.GetResult()->at(0).id != 4 || page.GetResult()->at(0).value != "   ")
        {
            return false;
        }
        page.SetPage(99, 10, 1);
        if (!page.Run(db) || page.GetTotal() != 3 || page.GetAnchorIndex() != SIZE_MAX || !page.GetResult()->empty()) {
            return false;
        }
        query::CategoryListQuery numeric(query::CategoryListQuery::MatchType::Substring, "rating", selections);
        numeric.SetTrackFilter("needle");
        numeric.SetPage(0, 0, 10);
        if (!numeric.Run(db) || numeric.GetTotal() != 1 || numeric.GetAnchorIndex() != 0 ||
            numeric.GetResult()->size() != 1 || numeric.GetResult()->at(0).id != 0)
        {
            return false;
        }
        query::CategoryListQuery duration(query::CategoryListQuery::MatchType::Substring, "duration", selections);
        duration.SetTrackFilter("needle");
        duration.SetPage(1, 0, 10);
        if (!duration.Run(db) || duration.GetTotal() != 1 || duration.GetAnchorIndex() != 0 ||
            duration.GetResult()->size() != 1 || duration.GetResult()->at(0).id != 1)
        {
            return false;
        }
        if (db.Execute("INSERT INTO meta_keys VALUES(2,'composer')") != db::Okay ||
            db.Execute("INSERT INTO meta_values VALUES(7,2,'Composer')") != db::Okay ||
            db.Execute("INSERT INTO track_meta VALUES(42,7)") != db::Okay) {
            return false;
        }
        query::CategoryListQuery custom(query::CategoryListQuery::MatchType::Substring, "composer",
            query::category::PredicateList { { "year", 5 } });
        custom.SetPage(-1, 0, 10);
        if (!custom.Run(db) || custom.GetTotal() != 1 || custom.GetResult()->size() != 1 ||
            custom.GetResult()->at(0).id != 7) {
            return false;
        }
        query::CategoryListQuery years(query::CategoryListQuery::MatchType::Substring, "year", selections);
        years.SetTrackFilter("needle");
        years.SetPage(5, 0, 10);
        if (!years.Run(db) || years.GetTotal() != 1 || years.GetAnchorIndex() != 0 ||
            years.GetResult()->size() != 1 || years.GetResult()->at(0).id != 5) {
            return false;
        }
        if (db.Execute("INSERT INTO genres VALUES(5,'',0)") != db::Okay ||
            db.Execute("INSERT INTO tracks(id,title,duration,visible,album_id,visual_artist_id,visual_genre_id,disc,track) VALUES(25,'needle',1,1,88,77,5,0,8)") != db::Okay) {
            return false;
        }
        page.SetPage(5, 0, 1);
        if (!page.Run(db) || page.GetTotal() != 4 || page.GetAnchorIndex() != 0 ||
            page.GetResult()->size() != 1 || !page.GetResult()->at(0).value.empty()) {
            return false;
        }
        if (db.Execute("DROP VIEW extended_metadata") != db::Okay) {
            return false;
        }
        query::CategoryListQuery invalid(query::CategoryListQuery::MatchType::Substring, "year");
        invalid.SetPage(-1, 0, 10);
        return !invalid.Run(db);
    }

    /* album query results cross the server wire with custom metadata intact. */
    bool checkAlbumWire() {
        using namespace musik::core::library::query;
        const auto original = Json::parse(
            R"({"result":[{"id":42,"value":"Album","type":"album","metadata":{"album_artist":"Artist","visual_album_artist_id":"17","thumbnail_id":"bad","custom_field":"value"}}]})");
        AlbumListQuery query;
        query.DeserializeResult(original.dump());
        const auto rows = query.GetResult();
        if (rows->size() != 1 || rows->front().id != 42 || rows->front().value != "Album" ||
            AlbumText(rows->front(), "custom_field") != "value" ||
            AlbumInteger(rows->front(), "visual_album_artist_id") != 17 ||
            AlbumInteger(rows->front(), "thumbnail_id") != 0)
        {
            return false;
        }
        return Json::parse(query.SerializeResult()) == original;
    }

    /* serve category replies on a real loopback connection. */
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
                        std::to_string(MUSIKCUBE_VERSION_MAJOR) + "." + std::to_string(MUSIKCUBE_VERSION_MINOR) + ".0";
                }
                else if (request.at("name") != "ping") {
                    const auto query = Json::parse(request.at("options").at("raw_query_data").get<std::string>());
                    ++this->commands;
                    if (query.at("name") == "TrackPageQuery") {
                        const auto& options = query.at("options");
                        this->valid = this->valid && options.at("filter") == "needle" &&
                            options.at("selections").dump() == R"([["artist",77],["album",88]])";
                        Json rows = Json::array();
                        if (options.at("offset") == 0) {
                            rows.push_back({ { "id", 42 }, { "values", Json::array({ Json::array({ "needle" }) }) } });
                        }
                        Json result { { "projectedPage",
                            { { "fields", options.at("fields") }, { "rows", rows }, { "total", 1 } } } };
                        reply["options"]["raw_query_data"] = result.dump();
                        this->Send(connection, reply);
                        return;
                    }
                    if (query.at("name") != "CategoryPageQuery") {
                        this->valid = false;
                        this->socket.close(connection, websocketpp::close::status::going_away, "unsupported query");
                        return;
                    }
                    const auto& options = query.at("options");
                    this->valid = this->valid && options.at("trackSearch") == "needle" &&
                        options.at("regularPredicateList").dump() == R"([["artist",77],["album",88]])";
                    if (this->failCategory) {
                        reply["options"]["success"] = false;
                    }
                    else {
                        const Json all = Json::parse(
                            R"([{"id":1,"value":"Same name","type":"genre"},{"id":3,"value":"Unknown","type":"genre"}])");
                        Json rows = Json::array();
                        const auto offset = options.at("offset").get<size_t>();
                        const auto limit = options.at("limit").get<size_t>();
                        for (size_t i = offset; i < all.size() && i - offset < limit; ++i) {
                            rows.push_back(all[i]);
                        }
                        const auto anchor = options.at("anchorId").get<int64_t>();
                        Json result { { "result", rows }, { "total", all.size() },
                            { "anchorIndex",
                                anchor == 1       ? 0
                                    : anchor == 3 ? 1
                                                  : -1 } };
                        if (this->malformed) {
                            result["total"] = "invalid";
                        }
                        reply["options"]["raw_query_data"] = result.dump();
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

        /* hold the next category reply to establish a cancellation or reconnect race. */
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
        std::atomic<bool> failCategory { false };
        std::atomic<bool> malformed { false };

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

}

/* exercise category queries, cancellation, failure and reconnects through the public API. */
int main() {
    using namespace musik::core;
    using namespace musik::core::api;
    if (!checkAlbumWire()) {
        std::fprintf(stderr, "album wire metadata did not round trip\n");
        return 1;
    }
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
    if (!wait([&] {
            return connected.has_value();
        }) ||
        connected != MC_OK)
    {
        return 1;
    }
    auto library = session.OpenLibrary();
    /* Submit the same constrained category query in each server state. */
    const auto submit = [&](Result& result, bool search, std::string_view text = "needle") {
        RelatedCategoriesQuery query;
        query.field = "genre";
        query.filter.trackFilter = search ? text : std::string_view {};
        query.filter.selections = { { "artist", 77 }, { "album", 88 } };
        return GetRelatedCategories(
            context, library, query, [&result](mc_result status, CategoryPage page, std::string_view) {
                ++result.calls;
                result.status = status;
                for (const auto& entry : page.entries) {
                    result.ids.push_back(entry.key);
                }
            });
    };
    /* a server without a capability flag must receive exactly one native query. */
    std::vector<CategoryEntry> retained;
    std::optional<mc_result> status;
    const auto receive = [&](mc_result result, CategoryPage page, std::string_view) {
        status = result;
        retained = std::move(page.entries);
    };
    try {
        RelatedCategoriesQuery invalid;
        invalid.field = "genre";
        invalid.filter.selections = { { "album", 1 }, { "album", 2 } };
        GetRelatedCategories(context, library, invalid, receive);
        return 1;
    }
    catch (const Error& error) {
        if (error.Result() != MC_ERROR_INVALID_ARGUMENT || status) {
            return 1;
        }
    }
    RelatedCategoriesQuery initial;
    initial.field = "genre";
    initial.filter.trackFilter = "needle";
    initial.filter.selections = { { "artist", 77 }, { "album", 88 } };
    GetRelatedCategories(context, library, initial, receive);
    if (!wait([&] {
            return status.has_value();
        }) ||
        status != MC_OK || retained.size() != 2 || retained[0].key != 1 || retained[0].value != "Same name" ||
        retained[1].key != 3 || retained[1].value != "Unknown" || !server.valid || server.commands != 1)
    {
        std::fprintf(stderr, "category values or request routing did not match\n");
        return 1;
    }
    server.failCategory = true;
    Result failed;
    submit(failed, true);
    if (!wait([&] {
            return failed.calls == 1;
        }) ||
        failed.status != MC_ERROR_IO || server.commands != 2)
    {
        return 1;
    }
    server.failCategory = false;
    server.Hold();
    Result canceled;
    const auto id = submit(canceled, true);
    if (!server.WaitHeld() || mc_request_cancel(context.Get(), id) != MC_OK || !wait([&] {
            return canceled.calls == 1;
        }) ||
        canceled.status != MC_ERROR_CANCELLED)
    {
        return 1;
    }
    server.Release();
    if (session.Disconnect() != MC_OK) {
        return 1;
    }
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) {
        connected = status;
    });
    if (!wait([&] {
            return connected.has_value();
        }) ||
        connected != MC_OK)
    {
        return 1;
    }
    library = session.OpenLibrary();
    /* retire an in-flight request with its connection before reconnecting. */
    server.Hold();
    Result disconnected;
    submit(disconnected, true);
    if (!server.WaitHeld() || session.Disconnect() != MC_OK || !wait([&] {
            return disconnected.calls == 1;
        }) ||
        disconnected.status != MC_ERROR_IO)
    {
        return 1;
    }
    connected.reset();
    session.Connect(context, [&](mc_result status, mc_remote_state, std::string_view) {
        connected = status;
    });
    if (!wait([&] {
            return connected.has_value();
        }) ||
        connected != MC_OK)
    {
        return 1;
    }
    library = session.OpenLibrary();
    server.Release();
    Result reconnected;
    submit(reconnected, true);
    if (!wait([&] {
            return reconnected.calls == 1;
        }) ||
        reconnected.status != MC_OK || reconnected.ids != std::vector<int64_t> { 1, 3 } || canceled.calls != 1 ||
        disconnected.calls != 1 || server.commands != 5 || !server.valid)
    {
        return 1;
    }
    std::optional<mc_result> pageStatus;
    std::vector<CategoryEntry> page;
    size_t total = 0, anchorIndex = SIZE_MAX;
    const auto pageReceived = [&](mc_result result, CategoryPage resultPage, std::string_view) {
        pageStatus = result;
        page = std::move(resultPage.entries);
        total = resultPage.total;
        anchorIndex = resultPage.anchorIndex.value_or(SIZE_MAX);
    };
    RelatedCategoriesQuery pageQuery;
    pageQuery.field = "genre";
    pageQuery.filter.trackFilter = "needle";
    pageQuery.filter.selections = { { "artist", 77 }, { "album", 88 } };
    pageQuery.anchorKey = 3;
    pageQuery.offset = 1;
    pageQuery.limit = 1;
    GetRelatedCategories(context, library, pageQuery, pageReceived);
    if (!wait([&] {
            return pageStatus.has_value();
        }) ||
        pageStatus != MC_OK || total != 2 || anchorIndex != 1 || page.size() != 1 || page[0].key != 3 ||
        page[0].value != "Unknown")
    {
        return 1;
    }
    pageStatus.reset();
    pageQuery.anchorKey = 99;
    pageQuery.offset = 20;
    GetRelatedCategories(context, library, pageQuery, pageReceived);
    if (!wait([&] {
            return pageStatus.has_value();
        }) ||
        pageStatus != MC_OK || total != 2 || anchorIndex != SIZE_MAX || !page.empty())
    {
        return 1;
    }
    server.malformed = true;
    Result malformed;
    submit(malformed, true);
    if (!wait([&] {
            return malformed.calls == 1;
        }) ||
        malformed.status != MC_ERROR_IO)
    {
        return 1;
    }
    server.malformed = false;
    Result recovered;
    submit(recovered, true);
    if (!wait([&] {
            return recovered.calls == 1;
        }) ||
        recovered.status != MC_OK || malformed.calls != 1)
    {
        return 1;
    }
    TrackQuery trackQuery;
    trackQuery.filter.trackFilter = "needle";
    trackQuery.filter.selections = { { "artist", 77 }, { "album", 88 } };
    trackQuery.fields = { MC_TRACK_FIELD_TITLE };
    trackQuery.limit = 1;
    std::optional<mc_result> trackStatus;
    TrackPage trackPage;
    QueryTracks(context, library, trackQuery, [&](mc_result status, TrackPage page, std::string_view) {
        trackStatus = status;
        trackPage = std::move(page);
    });
    if (!wait([&] {
            return trackStatus.has_value();
        }) ||
        trackStatus != MC_OK || trackPage.Total() != 1 || trackPage.Size() != 1 || trackPage.Id(0) != 42 ||
        trackPage.Track(0).Title() != "needle" || !server.valid)
    {
        return 1;
    }
    trackStatus.reset();
    trackQuery.offset = 10;
    QueryTracks(context, library, trackQuery, [&](mc_result status, TrackPage page, std::string_view) {
        trackStatus = status;
        trackPage = std::move(page);
    });
    if (!wait([&] {
            return trackStatus.has_value();
        }) ||
        trackStatus != MC_OK || trackPage.Total() != 1 || trackPage.Size())
    {
        return 1;
    }
    return context.Shutdown() == MC_OK ? 0 : 1;
}
