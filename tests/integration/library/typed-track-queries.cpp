#include <musikcore/db/SqliteExtensions.h>
#include <musikcore/db/ScopedQueryCancellation.h>
#include <musikcore/library.h>
#include <musikcore/library/query/TrackReadQueries.h>

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace {
    using namespace musik::core::library::query;

    enum TrackId { firstTrack = 30, secondTrack = 12, thirdTrack = 50, hiddenTrack = 4, missingTrack = 100 };

    constexpr auto fixture =
        "CREATE TABLE albums (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE artists (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE genres (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, visible INTEGER, duration INTEGER,"
        " album_id INTEGER, visual_artist_id INTEGER, visual_genre_id INTEGER, disc INTEGER, track INTEGER, source_id INTEGER);"
        "CREATE TABLE meta_keys (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE meta_values (id INTEGER PRIMARY KEY, meta_key_id INTEGER, sort_order INTEGER, content TEXT);"
        "CREATE TABLE track_meta (track_id INTEGER, meta_value_id INTEGER);"
        "INSERT INTO albums VALUES (1,'Blue'),(2,'Green');"
        "INSERT INTO artists VALUES (1,'Fixture Artist');"
        "INSERT INTO genres VALUES (1,'Ambient');"
        "INSERT INTO tracks VALUES (30,'Ocean',1,7,1,1,1,1,1,0),(12,'Harbor',1,3,1,1,1,1,2,7),"
        "(50,'Forest',1,11,2,1,1,1,1,7),(4,'Hidden',0,4,2,1,1,1,2,7);"
        "INSERT INTO meta_keys VALUES (1,'composer'),(2,'empty');"
        "INSERT INTO meta_values VALUES (1,1,20,'Second'),(2,1,10,'First'),(3,2,1,'');"
        "INSERT INTO track_meta VALUES (30,1),(30,2),(30,3);";

    /* Verify typed SQL search ordering, album boundaries, pagination, and bound injection-like filter text. */
    bool checkSearch(sqlite3 *database) {
        TrackSearchRows rows;
        const TrackSearchQuery artist("  fixture artist  ", TrackMatchMode::Substring, TrackSortType::Album);
        if (!artist.Run(database, rows) ||
            rows.ids != std::vector<int64_t>{ firstTrack, secondTrack, thirdTrack } ||
            rows.albumStarts != std::vector<bool>{ true, false, true } ||
            rows.albumDurations != std::map<size_t, uint64_t>{ { 0, 10 }, { 2, 11 } }) return false;

        /* Regex title sort has no album boundaries. Empty-search pagination still binds the range. */
        const TrackSearchQuery regex("Ocean|Forest", TrackMatchMode::Regex, TrackSortType::Title);
        if (!regex.Run(database, rows) || rows.ids != std::vector<int64_t>{ thirdTrack, firstTrack } ||
            !rows.albumStarts.empty() || !rows.albumDurations.empty()) return false;

        const TrackSearchQuery window("", TrackMatchMode::Substring, TrackSortType::Album, 1, 1);
        if (!window.Run(database, rows) || rows.ids != std::vector<int64_t>{ secondTrack } ||
            rows.albumDurations != std::map<size_t, uint64_t>{ { 0, 3 } }) return false;

        return TrackSearchQuery("' OR 1=1 --", TrackMatchMode::Substring, TrackSortType::Album).Run(database, rows) &&
            rows.ids.empty();
    }

    bool checkSourceSearch(sqlite3* database) {
        TrackSearchRows rows;
        TrackSearchQuery query("", TrackMatchMode::Substring, TrackSortType::Album);
        query.scope = LibraryQueryScope::ForSources({ 0 });
        if (!query.Run(database, rows) || rows.ids != std::vector<int64_t> { firstTrack } ||
            rows.albumDurations != std::map<size_t, uint64_t> { { 0, 7 } })
        {
            return false;
        }

        /* replacing a selection value cannot change the query's copied scope. */
        std::vector<int32_t> selected { 7, 0, 7 };
        auto currentScope = LibraryQueryScope::ForSources(selected);
        query.scope = currentScope;
        selected.clear();
        currentScope = LibraryQueryScope::ForSources({});
        if (query.scope.IsUnrestricted() || *query.scope.SourceIds() != std::vector<int32_t> { 0, 7 } ||
            !query.Run(database, rows) || rows.ids != std::vector<int64_t> { firstTrack, secondTrack, thirdTrack })
        {
            return false;
        }

        bool rejected = false;
        try {
            query.scope = LibraryQueryScope::ForSources({ -1 });
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected || !query.Run(database, rows) || rows.ids.size() != 3) {
            return false;
        }

        query.scope = LibraryQueryScope::ForSources({ 7, 7 });
        if (!query.Run(database, rows) || rows.ids != std::vector<int64_t> { secondTrack, thirdTrack } ||
            rows.albumStarts != std::vector<bool> { true, true } ||
            rows.albumDurations != std::map<size_t, uint64_t> { { 0, 3 }, { 1, 11 } })
        {
            return false;
        }

        /* filtering must happen before offset, not after taking a catalog page. */
        query.limit = 1;
        query.offset = 1;
        if (!query.Run(database, rows) || rows.ids != std::vector<int64_t> { thirdTrack }) {
            return false;
        }

        query.scope = LibraryQueryScope::ForSources({});
        if (!query.Run(database, rows) || !rows.ids.empty() || !rows.albumDurations.empty()) {
            return false;
        }

        query.scope = LibraryQueryScope::ForSources({ 0, 7 });
        query.limit = -1;
        query.offset = 0;
        query.filter = "Harbor";
        if (!query.Run(database, rows) || rows.ids != std::vector<int64_t> { secondTrack }) {
            return false;
        }

        query.scope = LibraryQueryScope::ForSources({ 99 });
        if (!query.Run(database, rows) || !rows.ids.empty()) {
            return false;
        }

        query.scope = LibraryQueryScope {};
        query.filter.clear();
        return query.scope.IsUnrestricted() && query.Run(database, rows) &&
            rows.ids == std::vector<int64_t> { firstTrack, secondTrack, thirdTrack };
    }

    bool checkAlbumIdentity(sqlite3* database) {
        constexpr auto albums =
            "INSERT INTO albums VALUES (3,'Blue'),(4,NULL),(5,NULL);"
            "INSERT INTO tracks VALUES (70,'Other ocean',1,5,3,1,1,1,1,7),"
            "(71,'Other harbor',1,6,3,1,1,1,2,7),"
            "(80,'Untitled first',1,2,4,1,1,1,1,0),"
            "(81,'Untitled second',1,3,5,1,1,1,1,0);";
        if (sqlite3_exec(database, albums, nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
        TrackSearchRows rows;
        if (!TrackSearchQuery("Blue", TrackMatchMode::Substring, TrackSortType::Album).Run(database, rows) ||
            rows.ids != std::vector<int64_t> { firstTrack, secondTrack, 70, 71 } ||
            rows.albumStarts != std::vector<bool> { true, false, true, false } ||
            rows.albumDurations != std::map<size_t, uint64_t> { { 0, 10 }, { 2, 11 } })
        {
            return false;
        }
        return TrackSearchQuery("Untitled", TrackMatchMode::Substring, TrackSortType::Album).Run(database, rows) &&
            rows.ids == std::vector<int64_t> { 80, 81 } && rows.albumStarts == std::vector<bool> { true, true } &&
            rows.albumDurations == std::map<size_t, uint64_t> { { 0, 2 }, { 1, 3 } };
    }

    /* a canceled read must interrupt CPU work and stop waiting for a writer. */
    bool checkQueryCancellation(sqlite3* database) {
        std::atomic_bool canceled { true };
        sqlite3_stmt* rawStatement = nullptr;
        if (sqlite3_prepare_v2(database,
                "WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<1000000) SELECT sum(x) FROM n",
                -1, &rawStatement, nullptr) != SQLITE_OK)
        {
            return false;
        }
        {
            musik::core::db::ScopedQueryCancellation control(database, canceled);
            const int result = sqlite3_step(rawStatement);
            sqlite3_finalize(rawStatement);
            if (result != SQLITE_INTERRUPT) {
                return false;
            }
        }
        const auto name =
            "sqlite-cancel-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db";
        sqlite3* writer = nullptr;
        sqlite3* reader = nullptr;
        bool passed = false;
        if (sqlite3_open(name.c_str(), &writer) == SQLITE_OK &&
            sqlite3_exec(writer, "CREATE TABLE values_table(id INTEGER); BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) ==
                SQLITE_OK &&
            sqlite3_open_v2(name.c_str(), &reader, SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr) ==
                SQLITE_OK)
        {
            musik::core::db::ScopedQueryCancellation control(reader, canceled);
            const auto start = std::chrono::steady_clock::now();
            sqlite3_stmt* statement = nullptr;
            const int result = sqlite3_prepare_v2(reader, "SELECT * FROM values_table", -1, &statement, nullptr);
            sqlite3_finalize(statement);
            passed = result == SQLITE_BUSY && std::chrono::steady_clock::now() - start < std::chrono::seconds(1);
        }
        sqlite3_close(reader);
        if (writer) {
            sqlite3_exec(writer, "ROLLBACK", nullptr, nullptr, nullptr);
        }
        sqlite3_close(writer);
        std::filesystem::remove(name);
        return passed;
    }

    /* Read inside an existing transaction so projection cleanup cannot accidentally commit the caller's
       work or reorder missing/multivalue metadata. */
    bool checkProjection(sqlite3* database) {
        const std::vector<std::string> fields = { "composer", MC_TRACK_FIELD_TITLE, "empty" };
        const int64_t ids[] = { firstTrack, hiddenTrack, missingTrack, secondTrack };
        const std::string text;
        const TrackPageQuery query(text, fields, 0, 4, ids);
        ProjectedTrackPage page;
        if (!query.Run(database, page) || sqlite3_get_autocommit(database) == 0 || page.names != fields ||
            page.rows.size() != 4)
        {
            return false;
        }
        if (page.rows[0].id != firstTrack ||
            page.rows[0].fields !=
                std::vector<std::vector<std::string>> { { "First", "Second" }, { "Ocean" }, { "" } } ||
            page.rows[1].id != hiddenTrack || !page.rows[1].fields[1].empty() || page.rows[2].id != missingTrack ||
            !page.rows[2].fields[1].empty() || page.rows[3].id != secondTrack ||
            page.rows[3].fields[1] != std::vector<std::string> { "Harbor" })
        {
            return false;
        }

        /* A caller's existing transaction remains open after a projected read. */
        if (sqlite3_exec(database, "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
        const bool preserved = query.Run(database, page) && sqlite3_get_autocommit(database) == 0;
        sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
        return preserved;
    }
}

/* Initialize an in-memory SQLite catalog and exercise the typed query implementation directly. */
int main() {
    sqlite3* raw = nullptr;
    const auto opened = sqlite3_open(":memory:", &raw);
    const auto database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>(raw, sqlite3_close);
    if (opened != SQLITE_OK || musik::core::db::SqliteExtensions::Register(raw) != SQLITE_OK ||
        sqlite3_exec(raw, fixture, nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        std::fputs("Could not create typed-query fixture\n", stderr);
        return 1;
    }
    if (!checkSearch(raw)) {
        std::fputs("Typed search failed\n", stderr);
        return 1;
    }
    if (!checkSourceSearch(raw)) {
        std::fputs("Source-scoped typed search failed\n", stderr);
        return 1;
    }
    if (!checkQueryCancellation(raw)) {
        std::fputs("SQLite cancellation failed\n", stderr);
        return 1;
    }
    if (!checkProjection(raw)) {
        std::fputs("Typed metadata projection failed\n", stderr);
        return 1;
    }
    if (!checkAlbumIdentity(raw)) {
        std::fputs("Album identity grouping failed\n", stderr);
        return 1;
    }
    return 0;
}
