#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <sqlite3.h>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

struct mc_track_page {
    struct Row {
        int64_t id;
        std::vector<std::vector<std::string>> fields;
    };
    std::vector<std::string> names;
    std::vector<Row> rows;
};

namespace {
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
    enum TagColumn { tag_track_id_column, tag_key_column, tag_value_column };
    enum TrackParameter { search_parameter = 1, limit_parameter, offset_parameter };
    constexpr int first_tag_parameter = 1;

    using TagFields = std::map<std::string, std::vector<size_t>>;
    using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

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
        std::unique_ptr<mc_track_page> &result) {
        sqlite3 *raw_database = nullptr;
        const int opened = sqlite3_open_v2(path.c_str(), &raw_database, SQLITE_OPEN_READONLY, nullptr);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw_database, sqlite3_close);
        if (opened != SQLITE_OK || sqlite3_exec(database.get(), "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
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
        sql += " FROM tracks t WHERE t.visible = 1 AND (?1 = '' OR instr(t.title, ?1) > 0) "
            "ORDER BY t.id LIMIT ?2 OFFSET ?3";

        auto statement = prepare(database.get(), sql);
        if (!statement || bind_string(statement.get(), search_parameter, search) != SQLITE_OK ||
            sqlite3_bind_int64(statement.get(), limit_parameter, limit) != SQLITE_OK ||
            sqlite3_bind_int64(statement.get(), offset_parameter, offset) != SQLITE_OK) return MC_ERROR_IO;

        /* Page tracks before expanding tags so repeated values never consume page slots. */
        std::unique_ptr<mc_track_page> page(new mc_track_page);
        page->names = fields;
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
        result = std::move(page);
        return MC_OK;
    }
}

mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context, const mc_track_query *query, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request) {
    using musik::core::ffi::context_data_directory;
    using musik::core::ffi::context_track_query_limits;
    using musik::core::ffi::exception_barrier;
    using musik::core::ffi::fail;
    using musik::core::ffi::read_option;
    using musik::core::ffi::submit_request;

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
        if ((search.length && !search.data) || (count && !fields) || count > limits.max_fields) {
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
        for (size_t i = 0; i < count; ++i) {
            if (!fields[i].data || !fields[i].length) return MC_ERROR_INVALID_ARGUMENT;
            names.emplace_back(fields[i].data, fields[i].length);
        }
        if (names.empty()) names.emplace_back(MC_TRACK_FIELD_TITLE);

        const auto page = std::make_shared<std::unique_ptr<mc_track_page>>();
        return submit_request(context, [=] {
            return query_tracks(path, term, names, offset, page_limit, *page);
        }, [=](mc_result result, void *data) {
            mc_error *error = nullptr;
            if (result == MC_ERROR_IO) fail(result, "Could not query musik.db", &error);
            const std::unique_ptr<mc_error, void (*)(mc_error *)> owned_error(error,
                [](mc_error *value) { mc_error_destroy(&value); });
            completion(result, result == MC_OK ? page->release() : nullptr, error, data);
        }, user_data, out_request);
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
