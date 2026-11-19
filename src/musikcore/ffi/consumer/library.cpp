#include <musikcore/library.h>
#include <musikcore/db/SqliteExtensions.h>
#include <musikcore/library/query/util/TrackSort.h>

#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <sqlite3.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

struct mc_track_page {
    struct Row {
        int64_t id;
        std::vector<std::vector<std::string>> fields;
    };
    std::vector<std::string> names;
    std::vector<Row> rows;
};

struct TrackResultData {
    std::string path;
    std::vector<int64_t> ids;
    std::vector<bool> album_starts;
};

struct mc_track_result {
    std::shared_ptr<const TrackResultData> data;
};

struct mc_track_ref {
    std::string path;
    int64_t id;
};

namespace {
    using namespace musik::core::ffi;
    namespace track_sort = musik::core::library::query;

    const std::map<std::string, const char *> core_fields = {
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

    enum TrackColumn { track_id_column, first_core_column };
    constexpr int search_album_column = 1;
    enum TagColumn { tag_track_id_column, tag_key_column, tag_value_column };
    enum TrackParameter { search_parameter = 1, limit_parameter, offset_parameter };
    constexpr int first_tag_parameter = 1;

    using TagFields = std::map<std::string, std::vector<size_t>>;
    using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
    using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

    Database open_database(const std::string &path) {
        sqlite3 *raw = nullptr;
        const int opened = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
        Database database(raw, sqlite3_close);
        if (opened != SQLITE_OK) database.reset();
        return database;
    }

    template<typename Result, typename Work, typename Completion>
    mc_result submit_library_request(mc_context *context, Work work, Completion completion,
        void *user_data, mc_request_id *out_request) {
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        return submit_request(context, [=] { return work(*output); }, [=](mc_result result, void *data) {
            mc_error *error = nullptr;
            if (result == MC_ERROR_IO) fail(result, "Could not query musik.db", &error);
            const std::unique_ptr<mc_error, void (*)(mc_error *)> owned_error(error,
                [](mc_error *value) { mc_error_destroy(&value); });
            completion(result, result == MC_OK ? output->release() : nullptr, error, data);
        }, user_data, out_request);
    }

    bool copy_fields(const mc_string_view *fields, size_t count, size_t maximum,
        std::vector<std::string> &names) {
        if ((count && !fields) || count > maximum) return false;
        for (size_t i = 0; i < count; ++i) {
            if (!fields[i].data || !fields[i].length) return false;
            names.emplace_back(fields[i].data, fields[i].length);
        }
        if (names.empty()) names.emplace_back(MC_TRACK_FIELD_TITLE);
        return true;
    }

    Statement prepare(sqlite3 *database, const std::string &sql) {
        sqlite3_stmt *statement = nullptr;
        sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr);
        return Statement(statement, sqlite3_finalize);
    }

    int bind_string(sqlite3_stmt *statement, int index, const std::string &value) {
        return sqlite3_bind_text64(statement, index, value.c_str(), value.size(), SQLITE_STATIC, SQLITE_UTF8);
    }

    bool read_string(sqlite3_stmt *statement, int column, std::string &value) {
        const auto *data = sqlite3_column_text(statement, column);
        if (!data) return false;
        value.assign(reinterpret_cast<const char *>(data), sqlite3_column_bytes(statement, column));
        return true;
    }

    std::string placeholders(size_t count) {
        std::string result = "?";
        for (size_t i = 1; i < count; ++i) result += ",?";
        return result;
    }

    mc_result load_tags(sqlite3 *database, const TagFields &fields, mc_track_page &page) {
        if (fields.empty() || page.rows.empty()) return MC_OK;

        /* Fetch only the selected track IDs and bind arbitrary tag keys as data. */
        auto statement = prepare(database,
            "SELECT DISTINCT tm.track_id, k.name, v.content, v.sort_order, v.id "
            "FROM track_meta tm JOIN meta_values v ON v.id = tm.meta_value_id "
            "JOIN meta_keys k ON k.id = v.meta_key_id WHERE v.content IS NOT NULL "
            "AND tm.track_id IN (" + placeholders(page.rows.size()) + ") "
            "AND k.name IN (" + placeholders(fields.size()) + ") "
            "ORDER BY tm.track_id, k.name, v.sort_order, v.id");
        if (!statement) return MC_ERROR_IO;

        int index = first_tag_parameter;
        for (const auto &row : page.rows) {
            if (sqlite3_bind_int64(statement.get(), index++, row.id) != SQLITE_OK) return MC_ERROR_IO;
        }
        for (const auto &field : fields) {
            if (bind_string(statement.get(), index++, field.first) != SQLITE_OK) return MC_ERROR_IO;
        }

        int status;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            const auto id = sqlite3_column_int64(statement.get(), tag_track_id_column);
            auto row = std::lower_bound(page.rows.begin(), page.rows.end(), id,
                [](const mc_track_page::Row &entry, int64_t key) { return entry.id < key; });
            std::string key;
            std::string value;
            if (!read_string(statement.get(), tag_key_column, key) || !read_string(statement.get(), tag_value_column, value)) return MC_ERROR_IO;
            const auto field = fields.find(key);
            if (row == page.rows.end() || row->id != id || field == fields.end()) return MC_ERROR_INTERNAL;
            for (const auto slot : field->second) row->fields[slot].push_back(value);
        }
        return status == SQLITE_DONE ? MC_OK : MC_ERROR_IO;
    }

    mc_result query_tracks(const std::string &path, const std::string &search,
        const std::vector<std::string> &fields, size_t offset, size_t limit,
        std::unique_ptr<mc_track_page> &result, const int64_t *ids = nullptr, bool keep_missing = true) {
        auto page = std::unique_ptr<mc_track_page>(new mc_track_page);
        page->names = fields;
        const auto database = open_database(path);
        if (!database || sqlite3_exec(database.get(), "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
            return MC_ERROR_IO;
        }

        /* Core projections use fixed expressions. Tags share the same read transaction. */
        std::string sql = "SELECT t." MC_TRACK_FIELD_ID;
        std::vector<size_t> columns;
        TagFields tags;
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto core = core_fields.find(fields[i]);
            if (core == core_fields.end()) {
                tags[fields[i]].push_back(i);
            } else {
                sql += ", ";
                sql += core->second;
                columns.push_back(i);
            }
        }
        sql += " FROM tracks t WHERE t.visible = 1 AND ";
        if (ids) sql += "t.id IN (" + placeholders(limit) + ") ORDER BY t.id";
        else sql += "(?1 = '' OR instr(t.title, ?1) > 0) ORDER BY t.id LIMIT ?2 OFFSET ?3";

        auto statement = prepare(database.get(), sql);
        if (!statement) return MC_ERROR_IO;
        if (ids) {
            for (size_t i = 0; i < limit; ++i) {
                if (sqlite3_bind_int64(statement.get(), static_cast<int>(i + 1), ids[i]) != SQLITE_OK) return MC_ERROR_IO;
            }
        } else if (bind_string(statement.get(), search_parameter, search) != SQLITE_OK ||
            sqlite3_bind_int64(statement.get(), limit_parameter, limit) != SQLITE_OK ||
            sqlite3_bind_int64(statement.get(), offset_parameter, offset) != SQLITE_OK) return MC_ERROR_IO;

        /* Page tracks before expanding tags so repeated values never consume page slots. */
        int status;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            mc_track_page::Row row{ sqlite3_column_int64(statement.get(), track_id_column), {} };
            row.fields.resize(fields.size());
            for (size_t i = 0; i < columns.size(); ++i) {
                const auto column = first_core_column + static_cast<int>(i);
                if (sqlite3_column_type(statement.get(), column) == SQLITE_NULL) continue;
                std::string value;
                if (!read_string(statement.get(), column, value)) return MC_ERROR_IO;
                row.fields[columns[i]].push_back(std::move(value));
            }
            page->rows.push_back(std::move(row));
        }
        if (status != SQLITE_DONE) return MC_ERROR_IO;

        const auto loaded = load_tags(database.get(), tags, *page);
        if (loaded != MC_OK) return loaded;
        if (ids && keep_missing) {
            /* SQL returns by ID. Restore snapshot order and leave removed rows empty. */
            auto sorted = std::move(page->rows);
            page->rows.clear();
            page->rows.reserve(limit);
            for (size_t i = 0; i < limit; ++i) {
                auto row = std::lower_bound(sorted.begin(), sorted.end(), ids[i],
                    [](const mc_track_page::Row &entry, int64_t id) { return entry.id < id; });
                if (row != sorted.end() && row->id == ids[i]) page->rows.push_back(std::move(*row));
                else page->rows.push_back({ ids[i], std::vector<std::vector<std::string>>(fields.size()) });
            }
        }
        result = std::move(page);
        return MC_OK;
    }

    mc_result open_search(const std::string &path, const std::string &text, bool regex,
        track_sort::TrackSortType order, std::unique_ptr<mc_track_result> &output) {
        const auto database = open_database(path);
        if (!database || musik::core::db::SqliteExtensions::Register(database.get()) != SQLITE_OK) {
            return MC_ERROR_IO;
        }

        /* Match the legacy filters and sort predicates using fixed SQL fragments. */
        std::string sql = "SELECT tracks.id, al.name FROM tracks "
            "JOIN albums al ON tracks.album_id=al.id "
            "JOIN artists ar ON tracks.visual_artist_id=ar.id "
            "JOIN genres gn ON tracks.visual_genre_id=gn.id WHERE tracks.visible=1";
        const auto predicate = track_sort::kTrackSearchSortOrderByPredicate.find(order);
        if (predicate != track_sort::kTrackSearchSortOrderByPredicate.end()) {
            sql += " AND (" + predicate->second + ")";
        }
        if (!text.empty()) {
            sql += regex
                ? " AND (tracks.title REGEXP ?1 OR al.name REGEXP ?1 OR ar.name REGEXP ?1 OR gn.name REGEXP ?1)"
                : " AND (tracks.title LIKE ?1 OR al.name LIKE ?1 OR ar.name LIKE ?1 OR gn.name LIKE ?1)";
        }
        sql += " ORDER BY " + track_sort::kTrackListSortOrderBy.at(order) + ", tracks.id";
        const auto statement = prepare(database.get(), sql);
        const std::string pattern = regex ? text : "%" + text + "%";
        if (!statement || (!text.empty() && bind_string(statement.get(), 1, pattern) != SQLITE_OK)) {
            return MC_ERROR_IO;
        }

        auto result = std::make_shared<TrackResultData>();
        result->path = path;
        const bool grouped = track_sort::kTrackSortTypeWithAlbumGrouping.count(order) != 0;
        std::string previous_album;
        int status;
        while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
            std::string album;
            if (grouped && !read_string(statement.get(), search_album_column, album) &&
                sqlite3_column_type(statement.get(), search_album_column) != SQLITE_NULL) return MC_ERROR_IO;
            result->album_starts.push_back(grouped && (result->ids.empty() || album != previous_album));
            previous_album = std::move(album);
            result->ids.push_back(sqlite3_column_int64(statement.get(), track_id_column));
        }
        if (status != SQLITE_DONE) return MC_ERROR_IO;
        output.reset(new mc_track_result{ std::move(result) });
        return MC_OK;
    }
}

mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context, const mc_track_query *query, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !query || !completion || query->struct_size < sizeof(query->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        /* Validate the requested projection before copying inputs for the worker. */
        const auto &limits = context_track_query_limits(*context);
        const auto search = read_option(query, &mc_track_query::search, offsetof(mc_track_query, search));
        const auto count = read_option(query, &mc_track_query::field_count, offsetof(mc_track_query, field_count));
        const auto fields = read_option(query, &mc_track_query::fields, offsetof(mc_track_query, fields));
        if (search.length && !search.data) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (read_option(query, &mc_track_query::flags, offsetof(mc_track_query, flags))) {
            return MC_ERROR_UNSUPPORTED;
        }

        const auto offset = read_option(query, &mc_track_query::offset, offsetof(mc_track_query, offset));
        const auto limit = read_option(query, &mc_track_query::limit, offsetof(mc_track_query, limit));
        const auto &root = context_data_directory(*context);
        if (offset > INT64_MAX || limit > limits.max_limit || root.empty()) return MC_ERROR_INVALID_ARGUMENT;
        const auto page_limit = limit ? limit : limits.default_limit;

        /* Snapshot inputs before submission. Cancellation leaves worker-owned data alone. */
        const std::string path = root + "/musik.db";
        const std::string term(search.length ? search.data : "", search.length);
        std::vector<std::string> names;
        if (!copy_fields(fields, count, limits.max_fields, names)) return MC_ERROR_INVALID_ARGUMENT;
        return submit_library_request<mc_track_page>(context,
            [=](std::unique_ptr<mc_track_page> &page) {
                return query_tracks(path, term, names, offset, page_limit, page);
            }, completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_open_track_search_async(mc_context *context, const mc_track_search *search,
    mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !search || !completion || search->struct_size < sizeof(search->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        const auto text = read_option(search, &mc_track_search::text, offsetof(mc_track_search, text));
        const auto mode = read_option(search, &mc_track_search::mode, offsetof(mc_track_search, mode));
        const auto order = read_option(search, &mc_track_search::order, offsetof(mc_track_search, order));
        if (mode > MC_TRACK_SEARCH_REGEX || order > MC_TRACK_SEARCH_GENRE) return MC_ERROR_UNSUPPORTED;
        const auto &root = context_data_directory(*context);
        if ((text.length && !text.data) || root.empty()) return MC_ERROR_INVALID_ARGUMENT;

        const std::string path = root + "/musik.db";
        std::string term(text.length ? text.data : "", text.length);
        if (mode == MC_TRACK_SEARCH_SUBSTRING && !term.empty()) {
            const auto begin = term.find_first_not_of(" \t\n\r\f\v");
            /* Keep a whitespace-only term equivalent to the legacy LIKE '%%'. */
            term = begin == std::string::npos ? "%" :
                term.substr(begin, term.find_last_not_of(" \t\n\r\f\v") - begin + 1);
        }
        return submit_library_request<mc_track_result>(context,
            [=](std::unique_ptr<mc_track_result> &result) {
                return open_search(path, term, mode == MC_TRACK_SEARCH_REGEX,
                    static_cast<track_sort::TrackSortType>(order), result);
            }, completion, user_data, out_request);
    });
}

size_t MC_CALL mc_track_result_get_count(const mc_track_result *tracks) {
    return tracks ? tracks->data->ids.size() : 0;
}

int64_t MC_CALL mc_track_result_get_id(const mc_track_result *tracks, size_t row) {
    return row < mc_track_result_get_count(tracks) ? tracks->data->ids[row] : 0;
}

bool MC_CALL mc_track_result_starts_album(const mc_track_result *tracks, size_t row) {
    return row < mc_track_result_get_count(tracks) && tracks->data->album_starts[row];
}

mc_result MC_CALL mc_track_result_read_async(mc_context *context, const mc_track_result *tracks,
    const mc_track_range *range, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !tracks || !range || !completion || range->struct_size < sizeof(range->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        const auto &limits = context_track_query_limits(*context);
        const auto offset = read_option(range, &mc_track_range::offset, offsetof(mc_track_range, offset));
        const auto limit = read_option(range, &mc_track_range::limit, offsetof(mc_track_range, limit));
        const auto count = read_option(range, &mc_track_range::field_count, offsetof(mc_track_range, field_count));
        const auto fields = read_option(range, &mc_track_range::fields, offsetof(mc_track_range, fields));
        std::vector<std::string> names;
        if (limit > limits.max_limit || !copy_fields(fields, count, limits.max_fields, names)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        /* Keep the snapshot alive even when the caller releases its handle. */
        const auto data = tracks->data;
        if (data->path != context_data_directory(*context) + "/musik.db") return MC_ERROR_INVALID_ARGUMENT;
        const auto start = std::min(offset, data->ids.size());
        const auto size = std::min(limit ? limit : limits.default_limit, data->ids.size() - start);
        return submit_library_request<mc_track_page>(context,
            [=](std::unique_ptr<mc_track_page> &page) -> mc_result {
                if (size) return query_tracks(data->path, "", names, 0, size, page, data->ids.data() + start);
                page.reset(new mc_track_page{ names, {} });
                return MC_OK;
            }, completion, user_data, out_request);
    });
}

void MC_CALL mc_track_result_destroy(mc_track_result **tracks) {
    if (!tracks) return;
    delete *tracks;
    *tracks = nullptr;
}

mc_result MC_CALL mc_track_result_get_ref(const mc_track_result *tracks, size_t row,
    mc_track_ref **out_track) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_track || *out_track || !tracks) return MC_ERROR_INVALID_ARGUMENT;
        if (row >= tracks->data->ids.size()) return MC_ERROR_NOT_FOUND;
        *out_track = new mc_track_ref{ tracks->data->path, tracks->data->ids[row] };
        return MC_OK;
    });
}

int64_t MC_CALL mc_track_ref_get_id(const mc_track_ref *track) {
    return track ? track->id : 0;
}

void MC_CALL mc_track_ref_destroy(mc_track_ref **track) {
    if (!track) return;
    delete *track;
    *track = nullptr;
}

mc_result MC_CALL mc_track_ref_read_async(mc_context *context, const mc_track_ref *track,
    const mc_string_view *fields, size_t field_count, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !track || !completion ||
            track->path != context_data_directory(*context) + "/musik.db") return MC_ERROR_INVALID_ARGUMENT;
        std::vector<std::string> names;
        if (!copy_fields(fields, field_count, context_track_query_limits(*context).max_fields, names)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        const auto identity = *track;
        return submit_library_request<mc_track_page>(context,
            [=](std::unique_ptr<mc_track_page> &page) -> mc_result {
                /* A direct identity lookup distinguishes a removed row from missing tags. */
                const auto status = query_tracks(identity.path, "", names, 0, 1, page, &identity.id, false);
                if (status != MC_OK) return status;
                return page->rows.empty() ? MC_ERROR_NOT_FOUND : MC_OK;
            }, completion, user_data, out_request);
    });
}

size_t MC_CALL mc_track_page_get_count(const mc_track_page *page) {
    return page ? page->rows.size() : 0;
}

int64_t MC_CALL mc_track_page_get_id(const mc_track_page *page, size_t row) {
    return row < mc_track_page_get_count(page) ? page->rows[row].id : 0;
}

size_t MC_CALL mc_track_page_get_field_count(const mc_track_page *page) {
    return page ? page->names.size() : 0;
}

mc_string_view MC_CALL mc_track_page_get_field_name(const mc_track_page *page, size_t field) {
    if (field >= mc_track_page_get_field_count(page)) return {};
    const auto &name = page->names[field];
    return { name.c_str(), name.size() };
}

size_t MC_CALL mc_track_page_get_value_count(const mc_track_page *page, size_t row, size_t field) {
    if (row >= mc_track_page_get_count(page) || field >= mc_track_page_get_field_count(page)) return 0;
    return page->rows[row].fields[field].size();
}

mc_string_view MC_CALL mc_track_page_get_string_at(
    const mc_track_page *page, size_t row, size_t field, size_t value) {
    if (value >= mc_track_page_get_value_count(page, row, field)) return {};
    const auto &text = page->rows[row].fields[field][value];
    return { text.c_str(), text.size() };
}

mc_string_view MC_CALL mc_track_page_get_string(const mc_track_page *page, size_t row, size_t field) {
    return mc_track_page_get_string_at(page, row, field, 0);
}

void MC_CALL mc_track_page_destroy(mc_track_page **page) {
    if (!page) return;
    delete *page;
    *page = nullptr;
}
