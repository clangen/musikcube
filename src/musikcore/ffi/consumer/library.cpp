#include <musikcore/library.h>

#include "library-storage.h"
#include "library-query-catalog.h"

#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace query = musik::core::library::query;

namespace {
    using namespace musik::core::ffi;

    bool copyFields(const mc_string_view *fields, size_t count, size_t maximum,
        std::vector<std::string> &names) {
        if ((count && !fields) || count > maximum || count > static_cast<size_t>(PTRDIFF_MAX) / sizeof(*fields)) return false;
        for (size_t i = 0; i < count; ++i) {
            if (!valid_text(fields[i], false)) return false;
            names.emplace_back(fields[i].data, fields[i].length);
        }
        if (names.empty()) names.emplace_back(MC_TRACK_FIELD_TITLE);
        return true;
    }


}

mc_result MC_CALL mc_library_create(const mc_library_options *options, mc_library **out_library) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_library || *out_library || !options || options->struct_size < sizeof(options->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto kind = read_option(options, &mc_library_options::kind, offsetof(mc_library_options, kind));
        const auto locator = read_option(options, &mc_library_options::locator, offsetof(mc_library_options, locator));
        if (kind != MC_LIBRARY_LOCAL && kind != MC_LIBRARY_REMOTE) return MC_ERROR_UNSUPPORTED;
        if (!valid_text(locator, false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        std::string target(locator.data, locator.length);
        auto instance = detail::CreateCatalogLibrary(std::move(target), kind);
        *out_library = new mc_library{ std::move(instance) };
        return MC_OK;
    });
}

void MC_CALL mc_library_destroy(mc_library **library) {
    if (!library) return;
    delete *library;
    *library = nullptr;
}

mc_result MC_CALL mc_context_set_library(mc_context* context, const mc_library* library) {
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        musik::core::ffi::LibraryStatus status;
        if (library) {
            status.kind = library->instance->Kind();
            status.configured = true;
            if (status.kind == MC_LIBRARY_LOCAL || library->instance->Matches(library->instance->NativeBackend().get()))
            {
                status.state = MC_LIBRARY_CONNECTED;
            }
        }
        return musik::core::ffi::set_context_library(*context, library ? library->instance : nullptr, std::move(status));
    });
}

mc_result MC_CALL mc_context_get_library(const mc_context* context, mc_library** out_library) {
    if (!context || !out_library || *out_library) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto instance = musik::core::ffi::context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        *out_library = new mc_library { std::move(instance) };
        return MC_OK;
    });
}

mc_result MC_CALL mc_library_list_albums_async(mc_context* context, const mc_library* library,
    mc_string_view filter, mc_album_list_completion_fn completion, void* user_data, mc_request_id* out_request)
{
    if (!out_request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *out_request = 0;
    if (!context || !completion || !valid_text(filter)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&]() -> mc_result {
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->ListAlbums(
            context, std::string(filter.data ? filter.data : "", filter.length), completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_read_thumbnail_async(mc_context* context, const mc_library* library,
    int64_t thumbnail_id, mc_thumbnail_completion_fn completion, void* user_data, mc_request_id* out_request)
{
    if (!out_request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *out_request = 0;
    if (!context || !completion || thumbnail_id <= 0) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&]() -> mc_result {
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->ReadThumbnailAsync(context, thumbnail_id, completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_track_set_rating_async(mc_context* context, const mc_track_ref* track, uint32_t rating,
    mc_library_completion_fn completion, void* user_data, mc_request_id* out_request)
{
    if (!out_request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *out_request = 0;
    if (!context || !track || !completion || rating > MC_TRACK_RATING_MAX) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&]() -> mc_result {
        return track->library->SetTrackRating(context, track->id, rating, completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_open_playlist_async(mc_context *context, const mc_library *library,
    int64_t playlist_id, mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !completion || playlist_id <= 0) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->OpenPlaylist(context, playlist_id, completion, user_data, out_request);
    });
}

static mc_result openSelection(mc_context *context, const mc_library *library, mc_string_view category,
    mc_string_view directory, int64_t id, mc_track_search_order order, mc_track_search_completion_fn completion,
    void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !completion || int(order) < 0 || order > MC_TRACK_SEARCH_DURATION) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto input = category.length ? category : directory;
        if (!valid_text(input, false)) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->OpenSelection(context, std::string(category.data ? category.data : "", category.length),
            std::string(directory.data ? directory.data : "", directory.length), id,
            static_cast<query::TrackSortType>(order), completion, data, request);
    });
}

mc_result MC_CALL mc_library_open_category_async(mc_context *context, const mc_library *library,
    mc_string_view category, int64_t id, mc_track_search_order order, mc_track_search_completion_fn completion,
    void *data, mc_request_id *request) {
    return openSelection(context, library, category, {}, id, order, completion, data, request);
}

mc_result MC_CALL mc_library_open_directory_async(mc_context *context, const mc_library *library,
    mc_string_view directory, mc_track_search_completion_fn completion, void *data, mc_request_id *request) {
    return openSelection(context, library, {}, directory, 0, MC_TRACK_SEARCH_ALBUM, completion, data, request);
}

mc_result MC_CALL mc_library_make_track_result(const mc_library *library, const int64_t *ids, size_t count,
    mc_track_result **out_result) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_result || *out_result) return MC_ERROR_INVALID_ARGUMENT;
        if (!library || !valid_ids(ids, count)) return MC_ERROR_INVALID_ARGUMENT;
        auto result = std::make_shared<TrackResultData>();
        result->library = library->instance;
        if (count) result->ids.assign(ids, ids + count);
        *out_result = new mc_track_result{std::move(result)};
        return MC_OK;
    });
}

mc_result MC_CALL mc_library_list_category_fields_async(mc_context* context, const mc_library* library,
    mc_category_fields_completion_fn completion, void* data, mc_request_id* request)
{
    if (!request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *request = 0;
    if (!context || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&]() -> mc_result {
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->ListCategoryFields(context, completion, data, request);
    });
}

mc_result MC_CALL mc_track_ref_get_library(const mc_track_ref* track, mc_library** out_library) {
    if (!track || !out_library || *out_library) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&] {
        *out_library = new mc_library { track->library };
        return MC_OK;
    });
}

mc_result MC_CALL mc_library_list_categories_async(mc_context* context, const mc_library* library,
    mc_string_view field, mc_string_view filter, mc_track_search_mode mode,
    mc_category_list_completion_fn completion, void* user_data, mc_request_id* out_request)
{
    if (!out_request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *out_request = 0;
    if (!context || !completion || !valid_text(field, false) || !valid_text(filter) ||
        mode > MC_TRACK_SEARCH_REGEX)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&]() -> mc_result {
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->ListCategories(context, std::string(field.data, field.length),
            std::string(filter.data ? filter.data : "", filter.length), mode, completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_list_facets_async(mc_context* context, const mc_library* library,
    mc_facet_field field, mc_string_view search, const mc_facet_selection* selections, size_t selectionCount,
    mc_category_list_completion_fn completion, void* data, mc_request_id* request)
{
    if (!request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *request = 0;
    if (!context || !completion || !valid_text(search) || field < MC_FACET_GENRE || field > MC_FACET_ALBUM ||
        selectionCount > 3 || (selectionCount && !selections))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&]() -> mc_result {
        constexpr const char* fields[] { "", "genre", "artist", "album" };
        bool seen[4] {};
        std::vector<std::pair<std::string, int64_t>> predicates;
        for (size_t i = 0; i < selectionCount; ++i) {
            const auto column = selections[i].field;
            const auto id = selections[i].id;
            if (column < MC_FACET_GENRE || column > MC_FACET_ALBUM || id <= 0 || seen[column]) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            seen[column] = true;
            predicates.emplace_back(fields[column], id);
        }
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        std::string term(search.data ? search.data : "", search.length);
        const auto begin = term.find_first_not_of(" \t\n\r\f\v");
        term = begin == std::string::npos ? "" : term.substr(begin, term.find_last_not_of(" \t\n\r\f\v") - begin + 1);
        return instance->ListFacets(
            context, fields[field], std::move(term), std::move(predicates), completion, data, request);
    });
}

mc_result MC_CALL mc_library_list_playlists_async(mc_context *context, const mc_library *library,
    mc_playlist_list_completion_fn completion, void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !completion) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->ListPlaylists(context, completion, data, request);
    });
}

mc_result MC_CALL mc_library_save_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, mc_string_view name, const int64_t *ids, size_t count, mc_playlist_write_completion_fn completion,
    void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id < 0 || (!id && !name.length) ||
            !valid_text(name) || !valid_ids(ids, count)) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library->instance;
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->SavePlaylist(context, id, std::string(name.data ? name.data : "", name.length),
            count ? std::vector<int64_t>(ids, ids + count) : std::vector<int64_t> {}, false, completion, data, request);
    });
}

mc_result MC_CALL mc_library_append_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, const int64_t *ids, size_t count, mc_playlist_write_completion_fn completion,
    void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id <= 0 || !valid_ids(ids, count)) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library->instance;
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->SavePlaylist(context, id, {},
            count ? std::vector<int64_t>(ids, ids + count) : std::vector<int64_t> {}, true, completion, data, request);
    });
}

mc_result MC_CALL mc_library_rename_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, mc_string_view name, mc_playlist_write_completion_fn completion, void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id <= 0 || !valid_text(name, false)) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library->instance;
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->RenamePlaylist(context, id, std::string(name.data, name.length), completion, data, request);
    });
}

mc_result MC_CALL mc_library_delete_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, mc_playlist_write_completion_fn completion, void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id <= 0) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library->instance;
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->DeletePlaylist(context, id, completion, data, request);
    });
}

mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context, const mc_library *library, const mc_track_query *query, mc_track_query_completion_fn completion,
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
        if (!valid_text(search)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (read_option(query, &mc_track_query::flags, offsetof(mc_track_query, flags))) {
            return MC_ERROR_UNSUPPORTED;
        }

        const auto offset = read_option(query, &mc_track_query::offset, offsetof(mc_track_query, offset));
        const auto limit = read_option(query, &mc_track_query::limit, offsetof(mc_track_query, limit));
        if (offset > INT64_MAX || limit > limits.max_limit) return MC_ERROR_INVALID_ARGUMENT;
        const auto page_limit = limit ? limit : limits.default_limit;

        /* Snapshot inputs before submission. Cancellation leaves worker-owned data alone. */
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        const std::string term(search.length ? search.data : "", search.length);
        std::vector<std::string> names;
        if (!copyFields(fields, count, limits.max_fields, names)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return instance->QueryTracks(
            context, query::TrackPageQuery(term, names, offset, page_limit), completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_open_track_search_async(mc_context *context, const mc_library *library,
    const mc_track_search *search,
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
        if (mode > MC_TRACK_SEARCH_REGEX || order > MC_TRACK_SEARCH_DURATION) return MC_ERROR_UNSUPPORTED;
        if (!valid_text(text)) return MC_ERROR_INVALID_ARGUMENT;
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        const std::string term(text.length ? text.data : "", text.length);
        return instance->Search(context,
            query::TrackSearchQuery(term, mode == MC_TRACK_SEARCH_REGEX, static_cast<query::TrackSortType>(order)),
            completion, user_data, out_request);
    });
}

size_t MC_CALL mc_track_result_get_count(const mc_track_result *tracks) {
    return tracks ? tracks->data->ids.size() : 0;
}

int64_t MC_CALL mc_track_result_get_id(const mc_track_result *tracks, size_t row) {
    return row < mc_track_result_get_count(tracks) ? tracks->data->ids[row] : 0;
}

bool MC_CALL mc_track_result_starts_album(const mc_track_result *tracks, size_t row) {
    return tracks && row < tracks->data->albumStarts.size() && tracks->data->albumStarts[row];
}

uint64_t MC_CALL mc_track_result_get_album_duration(const mc_track_result *tracks, size_t row) {
    if (!tracks) return 0;
    const auto &durations = tracks->data->albumDurations;
    const auto album = durations.find(row);
    return album == durations.end() ? 0 : album->second;
}

mc_result musik::core::ffi::read_track_result_scoped(mc_context *context, const mc_track_result *tracks,
    const mc_track_range *range, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request, const void* owner) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !tracks || !range || !completion || range->struct_size < sizeof(range->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        static const TrackQueryLimits pluginLimits {
            MC_TRACK_QUERY_DEFAULT_LIMIT,
            MC_TRACK_QUERY_MAX_LIMIT,
            MC_TRACK_QUERY_MAX_FIELDS,
        };
        const auto &limits = owner ? pluginLimits : context_track_query_limits(*context);
        const auto offset = read_option(range, &mc_track_range::offset, offsetof(mc_track_range, offset));
        const auto limit = read_option(range, &mc_track_range::limit, offsetof(mc_track_range, limit));
        const auto count = read_option(range, &mc_track_range::field_count, offsetof(mc_track_range, field_count));
        const auto fields = read_option(range, &mc_track_range::fields, offsetof(mc_track_range, fields));
        std::vector<std::string> names;
        if (limit > limits.max_limit || !copyFields(fields, count, limits.max_fields, names)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        /* Keep the snapshot alive even when the caller releases its handle. */
        const auto data = tracks->data;
        const auto start = std::min(offset, data->ids.size());
        const auto size = std::min(limit ? limit : limits.default_limit, data->ids.size() - start);
        return data->library->ReadPage(context, { data->ids.begin() + start, data->ids.begin() + start + size },
            std::move(names), true, completion, user_data, out_request, owner);
    });
}

mc_result MC_CALL mc_track_result_read_async(mc_context *context, const mc_track_result *tracks,
    const mc_track_range *range, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request)
{
    return musik::core::ffi::read_track_result_scoped(context, tracks, range, completion, user_data, out_request, nullptr);
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
        *out_track = new mc_track_ref{ tracks->data->library, tracks->data->ids[row] };
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
        if (!context || !track || !completion) return MC_ERROR_INVALID_ARGUMENT;
        std::vector<std::string> names;
        if (!copyFields(fields, field_count, context_track_query_limits(*context).max_fields, names)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        const auto instance = *track;
        return instance.library->ReadPage(
            context, { instance.id }, std::move(names), false, completion, user_data, out_request);
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

mc_result MC_CALL mc_library_list_sources_async(mc_context* context, const mc_library* library,
    mc_library_sources_completion_fn completion, void* data, mc_request_id* request)
{
    if (!request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *request = 0;
    if (!context || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&]() -> mc_result {
        const auto instance = library ? library->instance : context_library(*context);
        if (!instance) {
            return MC_ERROR_NOT_FOUND;
        }
        return instance->ListSources(context, completion, data, request);
    });
}

mc_result MC_CALL mc_library_set_scope(mc_library* library, const mc_library_scope* scope) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !scope || scope->struct_size < sizeof(scope->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (scope->struct_size > sizeof(*scope)) {
            return MC_ERROR_UNSUPPORTED;
        }
        query::LibraryQueryScope value;
        const auto* filter = read_option(scope, &mc_library_scope::sources, offsetof(mc_library_scope, sources));
        if (filter) {
            if (filter->struct_size < sizeof(*filter) || filter->count > MC_LIBRARY_SCOPE_MAX_SOURCES ||
                (filter->count && !filter->ids)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            if (filter->struct_size > sizeof(*filter)) {
                return MC_ERROR_UNSUPPORTED;
            }
            std::vector<int32_t> ids;
            for (size_t i = 0; i < filter->count; ++i) {
                if (filter->ids[i] < 0) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                ids.push_back(filter->ids[i]);
            }
            value = query::LibraryQueryScope::ForSources(std::move(ids));
        }
        const auto instance = library->instance;
        if (instance->GetScopeState()->Replace(std::move(value))) {
            const mc_library alias { instance };
            const mc_library_scope_change event { &alias };
            instance->GetScopeState()->Notify(event);
        }
        return MC_OK;
    });
}

mc_result MC_CALL mc_library_clear_scope(mc_library* library) {
    const mc_library_scope scope = MC_LIBRARY_SCOPE_DEFAULT;
    return mc_library_set_scope(library, &scope);
}

mc_result MC_CALL mc_library_get_scope(const mc_library* library, mc_library_scope_snapshot** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        *out = new mc_library_scope_snapshot(library->instance->GetScopeState()->Capture());
        return MC_OK;
    });
}

const mc_library_scope* MC_CALL mc_library_scope_read(const mc_library_scope_snapshot* scope) {
    return scope ? &scope->view : nullptr;
}

void MC_CALL mc_library_scope_destroy(mc_library_scope_snapshot** scope) {
    if (scope) {
        delete *scope;
        *scope = nullptr;
    }
}

mc_result MC_CALL mc_library_subscribe_scope(const mc_library* library, mc_event_fn callback,
    void* data, mc_subscription** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !callback || !out || *out) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return library->instance->GetScopeState()->Subscribe(callback, data, out);
    });
}

mc_result MC_CALL mc_library_create_playlist_async(mc_context* context, const mc_library* library,
    const mc_playlist_create_options* options, const int64_t* ids, size_t count,
    mc_playlist_write_completion_fn completion, void* data, mc_request_id* request)
{
    if (!request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *request = 0;
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!context || !library || !options || !completion || !valid_ids(ids, count) ||
            options->struct_size < sizeof(options->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto name = read_option(options, &mc_playlist_create_options::name, offsetof(mc_playlist_create_options, name));
        const auto ownership = read_option(options, &mc_playlist_create_options::ownership, offsetof(mc_playlist_create_options, ownership));
        if (!valid_text(name, false) || ownership > MC_PLAYLIST_SOURCE) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        std::optional<int32_t> owner;
        if (ownership == MC_PLAYLIST_SOURCE) {
            if (options->struct_size < offsetof(mc_playlist_create_options, source_id) + sizeof(options->source_id) || options->source_id < 0) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            owner = options->source_id;
        }
        return library->instance->SavePlaylist(context, 0, std::string(name.data, name.length),
            count ? std::vector<int64_t>(ids, ids + count) : std::vector<int64_t> {}, false, completion, data, request,
            owner);
    });
}
