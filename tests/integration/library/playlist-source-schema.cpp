#include <musikcore/db/Connection.h>
#include <musikcore/db/Statement.h>
#include <musikcore/library/LocalLibraryBackend.h>
#include <musikcore/library/query/SourceListQuery.h>
#include <musikcore/library.h>

#include <cstdio>
#include <cstdlib>

namespace {
    namespace db = musik::core::db;

    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "playlist source schema: %s\n", message);
            std::abort();
        }
    }

    void execute(db::Connection& database, const char* sql) {
        require(database.Execute(sql) == db::Okay, sql);
    }

    void checkOwnership(db::Connection& database) {
        require(database.Execute("SELECT source_id FROM playlists LIMIT 0") == db::Okay,
            "playlist ownership column must exist");
        execute(database, "INSERT INTO playlists (id,name,source_id) VALUES (42,'Files',0),(43,'Provider',7)");
        db::Statement rows("SELECT id,source_id FROM playlists ORDER BY id", database);
        require(rows.Step() == db::Row && rows.ColumnInt64(0) == 41 && rows.IsNull(1),
            "existing playlists must remain global");
        require(rows.Step() == db::Row && rows.ColumnInt64(0) == 42 && !rows.IsNull(1) && rows.ColumnInt32(1) == 0,
            "ordinary-file source must remain distinct from global");
        require(rows.Step() == db::Row && rows.ColumnInt64(0) == 43 && rows.ColumnInt32(1) == 7,
            "provider ownership must be retained");
        require(rows.Step() == db::Done, "ownership must not duplicate playlists");
        require(database.Execute("INSERT INTO playlists (name,source_id) VALUES ('Invalid',-1)") != db::Okay,
            "scan-source sentinels must not become playlist owners");
    }
}

int main() {
    db::Connection database;
    require(database.Open(":memory:") == db::Okay, "open catalog");
    musik::core::library::LocalLibraryBackend::CreateDatabase(database);
    execute(database, "INSERT INTO playlists (id,name,user_id) VALUES (41,'Existing',3)");
    checkOwnership(database);

    /* reproduce a version-11 catalog while retaining its current track schema. */
    execute(database, "DROP TRIGGER playlist_source_insert");
    execute(database, "DROP TRIGGER playlist_source_update");
    execute(database, "DROP TABLE playlists");
    execute(
        database, "CREATE TABLE playlists (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT,user_id INTEGER DEFAULT 0)");
    execute(database, "INSERT INTO playlists VALUES (41,'Existing',3)");
    execute(database, "INSERT INTO playlist_tracks VALUES (41,'track',7,0),(41,'track',7,1)");
    execute(database, "UPDATE version SET version=11");
    musik::core::library::LocalLibraryBackend::CreateDatabase(database);
    checkOwnership(database);
    musik::core::library::LocalLibraryBackend::CreateDatabase(database);

    execute(database, "INSERT INTO playlist_tracks VALUES (41,'file',0,2),(42,'file',0,0),(43,'provider',7,0)");
    require(database.Execute("INSERT INTO playlist_tracks VALUES (43,'other',0,1)") != db::Okay,
        "source playlist must reject a different source");
    require(database.Execute("INSERT INTO playlist_tracks VALUES (42,'provider',7,1)") != db::Okay,
        "file playlist must reject provider tracks");
    require(database.Execute("INSERT INTO playlist_tracks VALUES (43,'null',NULL,1)") != db::Okay,
        "source playlist must reject an unspecified track source");
    require(database.Execute("UPDATE playlist_tracks SET source_id=0 WHERE playlist_id=43") != db::Okay,
        "changing an entry must preserve owner membership");
    require(database.Execute("UPDATE playlist_tracks SET playlist_id=42 WHERE playlist_id=43") != db::Okay,
        "moving entries must validate the destination owner");
    require(database.Execute("UPDATE playlists SET source_id=0 WHERE id=43") != db::Okay,
        "changing ownership must validate existing entries");
    require(database.Execute("INSERT INTO playlist_tracks VALUES (43,'valid',7,1),(43,'invalid',0,2)") != db::Okay,
        "mixed insert must fail atomically");
    db::Statement count("SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id=43", database);
    require(count.Step() == db::Row && count.ColumnInt32(0) == 1, "failed mixed insert must preserve original entries");
    namespace query = musik::core::library::query;
    query::SourceListQuery sources;
    require(sources.Run(database), "list catalog sources");
    const auto rows = sources.GetResult();
    require(std::any_of(rows.begin(), rows.end(), [](const auto& row) {
        return row.id == 0 && row.label == "Music files";
    }), "discover ordinary files");
    require(std::any_of(rows.begin(), rows.end(), [](const auto& row) {
        return row.id == 7 && row.label == "Source 7";
    }), "discover unavailable sources from catalog references");
    auto remote = query::SourceListQuery::DeserializeQuery(sources.SerializeQuery());
    remote->DeserializeResult(sources.SerializeResult());
    require(remote->GetResult().size() == rows.size(), "source wire round trip");
    musik::core::library::LocalLibraryBackend::CreateDatabase(database);
    require(sources.Run(database) && sources.GetResult().size() == rows.size(),
        "retain referenced sources without a provider instance");
    mc_request_id request = 17;
    require(mc_library_list_sources_async(nullptr, nullptr, nullptr, nullptr, &request) == MC_ERROR_INVALID_ARGUMENT &&
        request == 0, "reject invalid discovery request before submission");
    db::Statement playlist("SELECT name,user_id,source_id FROM playlists WHERE id=41", database);
    require(playlist.Step() == db::Row && std::string(playlist.ColumnText(0)) == "Existing" &&
            playlist.ColumnInt32(1) == 3 && playlist.IsNull(2),
        "migration must preserve playlist data");
    db::Statement entries(
        "SELECT source_id,sort_order FROM playlist_tracks WHERE playlist_id=41 ORDER BY sort_order", database);
    for (int position = 0; position < 3; ++position) {
        require(entries.Step() == db::Row && entries.ColumnInt32(0) == (position < 2 ? 7 : 0) &&
                entries.ColumnInt32(1) == position,
            "migration must preserve duplicate entries and order");
    }
    require(entries.Step() == db::Done, "migration must preserve playlist entry count");
}
