#include <musikcore/db/SqliteExtensions.h>
#include <musikcore/library.h>
#include <musikcore/library/query/TrackReadQueries.h>

#include <sqlite3.h>

#include <cstdio>
#include <memory>

namespace {
    using namespace musik::core::library::query;

    enum TrackId { firstTrack = 30, secondTrack = 12, thirdTrack = 50, hiddenTrack = 4, missingTrack = 100 };

    constexpr auto fixture =
        "CREATE TABLE albums (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE artists (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE genres (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, visible INTEGER, duration INTEGER,"
        " album_id INTEGER, visual_artist_id INTEGER, visual_genre_id INTEGER, disc INTEGER, track INTEGER);"
        "CREATE TABLE meta_keys (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE meta_values (id INTEGER PRIMARY KEY, meta_key_id INTEGER, sort_order INTEGER, content TEXT);"
        "CREATE TABLE track_meta (track_id INTEGER, meta_value_id INTEGER);"
        "INSERT INTO albums VALUES (1,'Blue'),(2,'Green');"
        "INSERT INTO artists VALUES (1,'Fixture Artist');"
        "INSERT INTO genres VALUES (1,'Ambient');"
        "INSERT INTO tracks VALUES (30,'Ocean',1,7,1,1,1,1,1),(12,'Harbor',1,3,1,1,1,1,2),"
        "(50,'Forest',1,11,2,1,1,1,1),(4,'Hidden',0,4,2,1,1,1,2);"
        "INSERT INTO meta_keys VALUES (1,'composer'),(2,'empty');"
        "INSERT INTO meta_values VALUES (1,1,20,'Second'),(2,1,10,'First'),(3,2,1,'');"
        "INSERT INTO track_meta VALUES (30,1),(30,2),(30,3);";

    /* Verify typed SQL search ordering, album boundaries, pagination, and bound injection-like filter text. */
    bool checkSearch(sqlite3 *database) {
        TrackSearchRows rows;
        const TrackSearchQuery artist("  fixture artist  ", false, TrackSortType::Album);
        if (!artist.Run(database, rows) ||
            rows.ids != std::vector<int64_t>{ firstTrack, secondTrack, thirdTrack } ||
            rows.albumStarts != std::vector<bool>{ true, false, true } ||
            rows.albumDurations != std::map<size_t, uint64_t>{ { 0, 10 }, { 2, 11 } }) return false;

        /* Regex title sort has no album boundaries. Empty-search pagination still binds the range. */
        const TrackSearchQuery regex("Ocean|Forest", true, TrackSortType::Title);
        if (!regex.Run(database, rows) || rows.ids != std::vector<int64_t>{ thirdTrack, firstTrack } ||
            !rows.albumStarts.empty() || !rows.albumDurations.empty()) return false;

        const TrackSearchQuery window("", false, TrackSortType::Album, 1, 1);
        if (!window.Run(database, rows) || rows.ids != std::vector<int64_t>{ secondTrack } ||
            rows.albumDurations != std::map<size_t, uint64_t>{ { 0, 3 } }) return false;

        return TrackSearchQuery("' OR 1=1 --", false, TrackSortType::Album).Run(database, rows) &&
            rows.ids.empty();
    }

    /* Read inside an existing transaction so projection cleanup cannot accidentally commit the caller's
       work or reorder missing/multivalue metadata. */
    bool checkProjection(sqlite3 *database) {
        const std::vector<std::string> fields = { "composer", MC_TRACK_FIELD_TITLE, "empty" };
        const int64_t ids[] = { firstTrack, hiddenTrack, missingTrack, secondTrack };
        const std::string text;
        const TrackPageQuery query(text, fields, 0, 4, ids);
        ProjectedTrackPage page;
        if (!query.Run(database, page) || sqlite3_get_autocommit(database) == 0 ||
            page.names != fields || page.rows.size() != 4) return false;
        if (page.rows[0].id != firstTrack ||
            page.rows[0].fields != std::vector<std::vector<std::string>>{ { "First", "Second" }, { "Ocean" }, { "" } } ||
            page.rows[1].id != hiddenTrack || !page.rows[1].fields[1].empty() ||
            page.rows[2].id != missingTrack || !page.rows[2].fields[1].empty() ||
            page.rows[3].id != secondTrack || page.rows[3].fields[1] != std::vector<std::string>{ "Harbor" }) return false;

        /* A caller's existing transaction remains open after a projected read. */
        if (sqlite3_exec(database, "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) return false;
        const bool preserved = query.Run(database, page) && sqlite3_get_autocommit(database) == 0;
        sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
        return preserved;
    }
}

/* Initialize an in-memory SQLite catalog and exercise the typed query implementation directly. */
int main() {
    sqlite3 *raw = nullptr;
    const auto opened = sqlite3_open(":memory:", &raw);
    const auto database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>(raw, sqlite3_close);
    if (opened != SQLITE_OK || musik::core::db::SqliteExtensions::Register(raw) != SQLITE_OK ||
        sqlite3_exec(raw, fixture, nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::fputs("Could not create typed-query fixture\n", stderr);
        return 1;
    }
    if (!checkSearch(raw)) {
        std::fputs("Typed search failed\n", stderr);
        return 1;
    }
    if (!checkProjection(raw)) {
        std::fputs("Typed metadata projection failed\n", stderr);
        return 1;
    }
    return 0;
}
