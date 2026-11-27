#include "TrackReadQueries.h"
#include "util/CategoryQueryUtil.h"

#include <musikcore/library.h>
#include <sqlite3.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace musik { namespace core { namespace library { namespace query {

    namespace {
        const std::map<std::string, const char*> coreFields = {
            { MC_TRACK_FIELD_ID, "t." MC_TRACK_FIELD_ID },
            { MC_TRACK_FIELD_TITLE, "t." MC_TRACK_FIELD_TITLE },
            { MC_TRACK_FIELD_TRACK, "t." MC_TRACK_FIELD_TRACK },
            { MC_TRACK_FIELD_DISC, "t." MC_TRACK_FIELD_DISC },
            { MC_TRACK_FIELD_BPM, "t." MC_TRACK_FIELD_BPM },
            { MC_TRACK_FIELD_DURATION, "t." MC_TRACK_FIELD_DURATION },
            { MC_TRACK_FIELD_FILESIZE, "t." MC_TRACK_FIELD_FILESIZE },
            { MC_TRACK_FIELD_FILENAME, "t." MC_TRACK_FIELD_FILENAME },
            { MC_TRACK_FIELD_FILETIME, "t." MC_TRACK_FIELD_FILETIME },
            { MC_TRACK_FIELD_THUMBNAIL_ID, "t." MC_TRACK_FIELD_THUMBNAIL_ID },
            { MC_TRACK_FIELD_VISUAL_GENRE_ID, "t." MC_TRACK_FIELD_VISUAL_GENRE_ID },
            { MC_TRACK_FIELD_VISUAL_ARTIST_ID, "t." MC_TRACK_FIELD_VISUAL_ARTIST_ID },
            { MC_TRACK_FIELD_ALBUM_ARTIST_ID, "t." MC_TRACK_FIELD_ALBUM_ARTIST_ID },
            { MC_TRACK_FIELD_ALBUM_ID, "t." MC_TRACK_FIELD_ALBUM_ID },
            { MC_TRACK_FIELD_PATH_ID, "t." MC_TRACK_FIELD_PATH_ID },
            { MC_TRACK_FIELD_SOURCE_ID, "t." MC_TRACK_FIELD_SOURCE_ID },
            { MC_TRACK_FIELD_EXTERNAL_ID, "t." MC_TRACK_FIELD_EXTERNAL_ID },
            { MC_TRACK_FIELD_RATING, "t." MC_TRACK_FIELD_RATING },
            { MC_TRACK_FIELD_LAST_PLAYED, "t." MC_TRACK_FIELD_LAST_PLAYED },
            { MC_TRACK_FIELD_PLAY_COUNT, "t." MC_TRACK_FIELD_PLAY_COUNT },
            { MC_TRACK_FIELD_DATE_ADDED, "t." MC_TRACK_FIELD_DATE_ADDED },
            { MC_TRACK_FIELD_DATE_UPDATED, "t." MC_TRACK_FIELD_DATE_UPDATED },
            { MC_TRACK_FIELD_ALBUM, "(SELECT name FROM albums WHERE id = t.album_id)" },
            { MC_TRACK_FIELD_ARTIST, "(SELECT name FROM artists WHERE id = t.visual_artist_id)" },
            { MC_TRACK_FIELD_ALBUM_ARTIST, "(SELECT name FROM artists WHERE id = t.album_artist_id)" },
            { MC_TRACK_FIELD_GENRE, "(SELECT name FROM genres WHERE id = t.visual_genre_id)" },
            { MC_TRACK_FIELD_DIRECTORY, "(SELECT name FROM directories WHERE id = t.directory_id)" },
        };

        std::string selectionPredicate(
            const CategorySelection& selection, const char* alias, size_t index, size_t first) {
            const auto value = "?" + std::to_string(first + index * 2 + 1);
            const auto column = category::PREDICATE_TO_COLUMN_MAP.find(selection.field);
            if (column != category::PREDICATE_TO_COLUMN_MAP.end()) {
                return " AND " + std::string(alias) + "." + column->second + "=" + value;
            }
            const auto field = "?" + std::to_string(first + index * 2);
            return " AND EXISTS (SELECT 1 FROM track_meta tm JOIN meta_values mv ON mv.id=tm.meta_value_id "
                   "JOIN meta_keys mk ON mk.id=mv.meta_key_id WHERE tm.track_id=" +
                std::string(alias) + ".id AND mk.name=" + field + " AND mv.id=" + value + ")";
        }
        bool bindSelections(sqlite3_stmt* statement, const std::vector<CategorySelection>& selections, size_t first) {
            for (size_t i = 0; i < selections.size(); ++i) {
                if (sqlite3_bind_int64(statement, static_cast<int>(first + i * 2 + 1), selections[i].key) != SQLITE_OK)
                {
                    return false;
                }
                if (category::PREDICATE_TO_COLUMN_MAP.find(selections[i].field) ==
                        category::PREDICATE_TO_COLUMN_MAP.end() &&
                    sqlite3_bind_text64(statement, static_cast<int>(first + i * 2), selections[i].field.c_str(),
                        selections[i].field.size(), SQLITE_TRANSIENT, SQLITE_UTF8) != SQLITE_OK)
                {
                    return false;
                }
            }
            return true;
        }

        enum TrackColumn { trackIdColumn, firstCoreColumn };
        constexpr int searchAlbumColumn = 1;
        constexpr int searchDurationColumn = 2;
        enum TagColumn { tagTrackIdColumn, tagKeyColumn, tagValueColumn };
        enum TrackParameter { searchParameter = 1, limitParameter, offsetParameter };
        constexpr int firstTagParameter = 1;
        constexpr int firstSourceParameter = offsetParameter + 1;

        using TagFields = std::map<std::string, std::vector<size_t>>;
        using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
        Statement prepare(sqlite3* database, const std::string& sql) {
            sqlite3_stmt* statement = nullptr;
            sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr);
            return Statement(statement, sqlite3_finalize);
        }

        int bindString(sqlite3_stmt* statement, int index, const std::string& value) {
            return sqlite3_bind_text64(statement, index, value.c_str(), value.size(), SQLITE_STATIC, SQLITE_UTF8);
        }

        bool readString(sqlite3_stmt* statement, int column, std::string& value) {
            const auto* data = sqlite3_column_text(statement, column);
            if (!data) {
                return false;
            }
            value.assign(reinterpret_cast<const char*>(data), sqlite3_column_bytes(statement, column));
            return true;
        }

        std::string placeholders(size_t count) {
            std::string result = "?";
            for (size_t i = 1; i < count; ++i) {
                result += ",?";
            }
            return result;
        }

        bool loadTags(sqlite3* database, const TagFields& fields, ProjectedTrackPage& page) {
            if (fields.empty() || page.rows.empty()) {
                return true;
            }

            /* Fetch only the selected track IDs and bind arbitrary tag keys as data. */
            auto statement = prepare(database,
                "SELECT DISTINCT tm.track_id, k.name, v.content, v.sort_order, v.id "
                "FROM track_meta tm JOIN meta_values v ON v.id = tm.meta_value_id "
                "JOIN meta_keys k ON k.id = v.meta_key_id WHERE v.content IS NOT NULL "
                "AND tm.track_id IN (" +
                    placeholders(page.rows.size()) +
                    ") "
                    "AND k.name IN (" +
                    placeholders(fields.size()) +
                    ") "
                    "ORDER BY tm.track_id, k.name, v.sort_order, v.id");
            if (!statement) {
                return false;
            }

            int index = firstTagParameter;
            for (const auto& row : page.rows) {
                if (sqlite3_bind_int64(statement.get(), index++, row.id) != SQLITE_OK) {
                    return false;
                }
            }
            for (const auto& field : fields) {
                if (bindString(statement.get(), index++, field.first) != SQLITE_OK) {
                    return false;
                }
            }

            int status;
            while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
                const auto id = sqlite3_column_int64(statement.get(), tagTrackIdColumn);
                auto row = std::lower_bound(
                    page.rows.begin(), page.rows.end(), id, [](const ProjectedTrackPage::Row& entry, int64_t key) {
                        return entry.id < key;
                    });
                std::string key;
                std::string value;
                if (!readString(statement.get(), tagKeyColumn, key) ||
                    !readString(statement.get(), tagValueColumn, value))
                {
                    return false;
                }
                const auto field = fields.find(key);
                if (row == page.rows.end() || row->id != id || field == fields.end()) {
                    return false;
                }
                for (const auto slot : field->second) {
                    row->fields[slot].push_back(value);
                }
            }
            return status == SQLITE_DONE;
        }


        /* A caller that already owns a transaction keeps responsibility for its lifetime. */
        class ReadTransaction {
        public:
            explicit ReadTransaction(sqlite3* database)
            : database(database)
            , owns(sqlite3_get_autocommit(database) != 0)
            , ready(!owns || sqlite3_exec(database, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK) {}
            ~ReadTransaction() {
                if (owns && ready) {
                    sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
                }
            }
            ReadTransaction(const ReadTransaction&) = delete;
            ReadTransaction& operator=(const ReadTransaction&) = delete;

            sqlite3* const database;
            const bool owns;
            const bool ready;
        };
    }

    bool TrackSearchQuery::Run(sqlite3* database, TrackSearchRows& result) const {
        /* Both library backends use the same matching, ordering and grouping rules. */
        const auto sort = kTrackListSortOrderBy.find(order);
        if (sort == kTrackListSortOrderBy.end()) {
            return false;
        }
        if (this->scope.MatchesNoTracks()) {
            result = {};
            return true;
        }
        std::string sql =
            "SELECT tracks.id, tracks.album_id, tracks.duration FROM tracks "
            "LEFT JOIN albums al ON tracks.album_id=al.id "
            "LEFT JOIN artists ar ON tracks.visual_artist_id=ar.id "
            "LEFT JOIN genres gn ON tracks.visual_genre_id=gn.id WHERE tracks.visible=1";
        if (!filter.empty()) {
            sql += matchMode == TrackMatchMode::Regex
                ? " AND (tracks.title REGEXP ?1 OR al.name REGEXP ?1 OR ar.name REGEXP ?1 OR gn.name REGEXP ?1)"
                : " AND (tracks.title LIKE ?1 ESCAPE '\\' OR al.name LIKE ?1 ESCAPE '\\' OR ar.name LIKE ?1 ESCAPE '\\' OR gn.name LIKE ?1 ESCAPE '\\')";
        }
        const size_t firstSelection =
            firstSourceParameter + (this->scope.SourceIds() ? this->scope.SourceIds()->size() : 0);
        for (size_t i = 0; i < selections.size(); ++i) {
            sql += selectionPredicate(selections[i], "tracks", i, firstSelection);
        }
        const auto& sourceIds = this->scope.SourceIds();
        if (sourceIds) {
            sql += " AND tracks.source_id IN (";
            for (size_t i = 0; i < sourceIds->size(); ++i) {
                if (i != 0) {
                    sql += ",";
                }
                sql += "?" + std::to_string(firstSourceParameter + i);
            }
            sql += ")";
        }
        sql += " ORDER BY " + sort->second + ", tracks.id";
        const bool paged = limit > 0 && offset >= 0;
        if (paged) {
            sql += " LIMIT ?2 OFFSET ?3";
        }

        /* Bind the filter and range instead of interpolating input into SQL. */
        std::string pattern = filter;
        if (matchMode == TrackMatchMode::Substring && !filter.empty()) {
            const auto begin = filter.find_first_not_of(" \t\n\r\f\v");
            pattern = begin == std::string::npos
                ? "%%"
                : category::SubstringPattern(filter.substr(begin, filter.find_last_not_of(" \t\n\r\f\v") - begin + 1));
        }
        const auto statement = prepare(database, sql);
        if (!statement || (!filter.empty() && bindString(statement.get(), searchParameter, pattern) != SQLITE_OK) ||
            (paged &&
                (sqlite3_bind_int(statement.get(), limitParameter, limit) != SQLITE_OK ||
                    sqlite3_bind_int(statement.get(), offsetParameter, offset) != SQLITE_OK)))
        {
            return false;
        }

        if (sourceIds) {
            for (size_t i = 0; i < sourceIds->size(); ++i) {
                if (sqlite3_bind_int64(statement.get(), static_cast<int>(firstSourceParameter + i), (*sourceIds)[i]) !=
                    SQLITE_OK)
                {
                    return false;
                }
            }
        }

        if (!bindSelections(statement.get(), selections,
                firstSourceParameter + (this->scope.SourceIds() ? this->scope.SourceIds()->size() : 0)))
        {
            return false;
        }

        TrackSearchRows rows;
        const bool grouped = kTrackSortTypeWithAlbumGrouping.count(order) != 0;
        int64_t previousAlbum = 0;
        auto currentAlbum = rows.albumDurations.end();
        int status;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            if (grouped) {
                const auto album = sqlite3_column_int64(statement.get(), searchAlbumColumn);
                const bool startsAlbum = rows.ids.empty() || album != previousAlbum;
                rows.albumStarts.push_back(startsAlbum);
                if (startsAlbum) {
                    currentAlbum = rows.albumDurations.emplace(rows.ids.size(), 0).first;
                }
                previousAlbum = album;
                const auto seconds = sqlite3_column_int64(statement.get(), searchDurationColumn);
                if (seconds > 0) {
                    currentAlbum->second += static_cast<uint64_t>(seconds);
                }
            }
            rows.ids.push_back(sqlite3_column_int64(statement.get(), trackIdColumn));
        }
        if (status != SQLITE_DONE) {
            return false;
        }
        result = std::move(rows);
        return true;
    }

    bool TrackPageQuery::Run(sqlite3* database, ProjectedTrackPage& result) const {
        ProjectedTrackPage page;
        page.names = fields;
        if (!ids && includeTotal) {
            page.total = 0;
        }
        if (!ids && this->scope.MatchesNoTracks()) {
            result = std::move(page);
            return true;
        }
        const ReadTransaction transaction(database);
        if (!transaction.ready) {
            return false;
        }

        /* Core projections use fixed expressions. Tags share the same read transaction. */
        std::string sql = "SELECT t." MC_TRACK_FIELD_ID;
        std::vector<size_t> columns;
        TagFields tags;
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto core = coreFields.find(fields[i]);
            if (core == coreFields.end()) {
                tags[fields[i]].push_back(i);
            }
            else {
                sql += ", ";
                sql += core->second;
                columns.push_back(i);
            }
        }
        if (ids) {
            sql += visibility == TrackVisibility::IncludeHidden ? " FROM tracks t WHERE "
                                                                : " FROM tracks t WHERE t.visible = 1 AND ";
            sql += "t.id IN (" + placeholders(limit) + ") ORDER BY t.id";
        }
        else {
            const std::string op = matchMode == TrackMatchMode::Regex ? "REGEXP" : "LIKE";
            const std::string escape = matchMode == TrackMatchMode::Regex ? "" : " ESCAPE '\\'";
            std::string matching = "SELECT t.id FROM tracks t WHERE t.visible = 1 AND (?1 = '' OR (t.title " + op +
                " ?1" + escape + " OR "
                "(SELECT name FROM albums WHERE id=t.album_id) " +
                op +
                " ?1" + escape + " OR "
                "(SELECT name FROM artists WHERE id=t.visual_artist_id) " +
                op +
                " ?1" + escape + " OR "
                "(SELECT name FROM genres WHERE id=t.visual_genre_id) " +
                op + " ?1" + escape + "))";
            const size_t firstSelection =
                firstSourceParameter + (this->scope.SourceIds() ? this->scope.SourceIds()->size() : 0);
            for (size_t i = 0; i < selections.size(); ++i) {
                matching += selectionPredicate(selections[i], "t", i, firstSelection);
            }
            if (this->scope.SourceIds()) {
                const auto& selected = *this->scope.SourceIds();
                matching += " AND t.source_id IN (";
                for (size_t i = 0; i < selected.size(); ++i) {
                    if (i != 0) {
                        matching += ",";
                    }
                    matching += "?" + std::to_string(firstSourceParameter + i);
                }
                matching += ")";
            }
            if (includeTotal) {
                /* A single statement keeps the total and page in one snapshot,
                   including when the requested offset is past the last row. */
                sql = "WITH matching AS MATERIALIZED (" + matching + ") " + sql +
                    ", totals.count FROM (SELECT count(*) AS count FROM matching) totals "
                    "LEFT JOIN (SELECT id FROM matching ORDER BY id LIMIT ?2 OFFSET ?3) selected ON 1 "
                    "LEFT JOIN tracks t ON t.id=selected.id ORDER BY selected.id";
            }
            else {
                sql += " FROM tracks t WHERE t.id IN (" + matching + ") ORDER BY t.id LIMIT ?2 OFFSET ?3";
            }
        }

        const std::string pattern = search.empty() || matchMode == TrackMatchMode::Regex ? search : category::SubstringPattern(search);
        auto statement = prepare(database, sql);
        if (!statement) {
            return false;
        }
        if (ids) {
            for (size_t i = 0; i < limit; ++i) {
                if (sqlite3_bind_int64(statement.get(), static_cast<int>(i + 1), ids[i]) != SQLITE_OK) {
                    return false;
                }
            }
        }
        else if (bindString(statement.get(), searchParameter, pattern) != SQLITE_OK ||
            sqlite3_bind_int64(statement.get(), limitParameter, limit) != SQLITE_OK ||
            sqlite3_bind_int64(statement.get(), offsetParameter, offset) != SQLITE_OK)
        {
            return false;
        }

        if (!ids && this->scope.SourceIds()) {
            const auto& selected = *this->scope.SourceIds();
            for (size_t i = 0; i < selected.size(); ++i) {
                if (sqlite3_bind_int64(statement.get(), static_cast<int>(firstSourceParameter + i), selected[i]) !=
                    SQLITE_OK)
                {
                    return false;
                }
            }
        }

        /* Page tracks before expanding tags so repeated values never consume page slots. */
        if (!ids &&
            !bindSelections(statement.get(), selections,
                firstSourceParameter + (this->scope.SourceIds() ? this->scope.SourceIds()->size() : 0)))
        {
            return false;
        }
        int status;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            if (!ids && includeTotal) {
                page.total = static_cast<size_t>(
                    sqlite3_column_int64(statement.get(), sqlite3_column_count(statement.get()) - 1));
                if (sqlite3_column_type(statement.get(), trackIdColumn) == SQLITE_NULL) {
                    continue;
                }
            }
            ProjectedTrackPage::Row row { sqlite3_column_int64(statement.get(), trackIdColumn), {} };
            row.fields.resize(fields.size());
            for (size_t i = 0; i < columns.size(); ++i) {
                const auto column = firstCoreColumn + static_cast<int>(i);
                if (sqlite3_column_type(statement.get(), column) == SQLITE_NULL) {
                    continue;
                }
                std::string value;
                if (!readString(statement.get(), column, value)) {
                    return false;
                }
                row.fields[columns[i]].push_back(std::move(value));
            }
            page.rows.push_back(std::move(row));
        }
        if (status != SQLITE_DONE) {
            return false;
        }

        if (!loadTags(database, tags, page)) {
            return false;
        }
        if (ids && keepMissing) {
            /* SQL returns by ID. Restore snapshot order and leave removed rows empty. */
            auto sorted = std::move(page.rows);
            std::vector<size_t> restored(sorted.size(), SIZE_MAX);
            page.rows.clear();
            page.rows.reserve(limit);
            for (size_t i = 0; i < limit; ++i) {
                auto row = std::lower_bound(
                    sorted.begin(), sorted.end(), ids[i], [](const ProjectedTrackPage::Row& entry, int64_t id) {
                        return entry.id < id;
                    });
                if (row == sorted.end() || row->id != ids[i]) {
                    page.rows.push_back({ ids[i], std::vector<std::vector<std::string>>(fields.size()) });
                    continue;
                }

                /* One SQL row can represent several queue entries with the same ID. */
                auto& first = restored[static_cast<size_t>(row - sorted.begin())];
                if (first == SIZE_MAX) {
                    first = page.rows.size();
                    page.rows.push_back(std::move(*row));
                }
                else {
                    page.rows.push_back(page.rows[first]);
                }
            }
        }
        result = std::move(page);
        return true;
    }

} } } }