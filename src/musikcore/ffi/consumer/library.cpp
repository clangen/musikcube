#include <musikcore/library.h>

#include "library-storage.h"

#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace {
    using namespace musik::core::ffi;

    template<typename Result, typename Work, typename Completion>
    mc_result submitLibraryRequest(mc_context *context, Work work, Completion completion,
        void *user_data, mc_request_id *out_request) {
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        return submit_request(context, [=] { return work(*output); }, [=](mc_result result, void *data) {
            mc_error *error = nullptr;
            if (result == MC_ERROR_IO) fail(result, "Could not query the library", &error);
            const std::unique_ptr<mc_error, void (*)(mc_error *)> owned_error(error,
                [](mc_error *value) { mc_error_destroy(&value); });
            completion(result, result == MC_OK ? output->release() : nullptr, error, data);
        }, user_data, out_request);
    }

    bool copyFields(const mc_string_view *fields, size_t count, size_t maximum,
        std::vector<std::string> &names) {
        if ((count && !fields) || count > maximum) return false;
        for (size_t i = 0; i < count; ++i) {
            if (!fields[i].data || !fields[i].length) return false;
            names.emplace_back(fields[i].data, fields[i].length);
        }
        if (names.empty()) names.emplace_back(MC_TRACK_FIELD_TITLE);
        return true;
    }

    mc_result readPage(const LibraryIdentity &library, const query::TrackPageQuery &query,
        std::unique_ptr<mc_track_page> &output) {
        auto page = std::unique_ptr<mc_track_page>(new mc_track_page);
        if (!library.backend->Run(query, *page)) return MC_ERROR_IO;
        output = std::move(page);
        return MC_OK;
    }

    mc_result openSearch(const std::shared_ptr<const LibraryIdentity> &library, const query::TrackSearchQuery &query,
        std::unique_ptr<mc_track_result> &output) {
        auto result = std::make_shared<TrackResultData>();
        if (!library->backend->Run(query, *result)) return MC_ERROR_IO;
        result->library = library;
        output.reset(new mc_track_result{ std::move(result) });
        return MC_OK;
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
        if (!locator.data || !locator.length || std::memchr(locator.data, '\0', locator.length)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        std::string target(locator.data, locator.length);
        auto backend = kind == MC_LIBRARY_LOCAL ? query::OpenTrackReadBackend(target + "/musik.db") : nullptr;
        auto identity = std::make_shared<LibraryIdentity>(LibraryIdentity{ std::move(target), std::move(backend), {}, {}, {} });
        *out_library = new mc_library{ std::move(identity) };
        return MC_OK;
    });
}

void MC_CALL mc_library_destroy(mc_library **library) {
    if (!library) return;
    delete *library;
    *library = nullptr;
}

mc_result MC_CALL mc_library_open_playlist_async(mc_context *context, const mc_library *library,
    int64_t playlist_id, mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !library || !completion || playlist_id <= 0) return MC_ERROR_INVALID_ARGUMENT;
        const auto identity = library->identity;
        if (!identity->playlists) return MC_ERROR_UNSUPPORTED;
        return identity->playlists->OpenPlaylist(context, identity, playlist_id, completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_list_playlists_async(mc_context *context, const mc_library *library,
    mc_playlist_list_completion_fn completion, void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion) return MC_ERROR_INVALID_ARGUMENT;
        const auto reader = library->identity->playlists;
        return reader ? reader->ListPlaylists(context, completion, data, request) : MC_ERROR_UNSUPPORTED;
    });
}

mc_result MC_CALL mc_library_save_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, mc_string_view name, const int64_t *ids, size_t count, mc_playlist_write_completion_fn completion,
    void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id < 0 || (!id && !name.length) ||
            (name.length && (!name.data || std::memchr(name.data, '\0', name.length))) ||
            (count && !ids)) return MC_ERROR_INVALID_ARGUMENT;
        const auto reader = library->identity->playlists;
        if (!reader) return MC_ERROR_UNSUPPORTED;
        return reader->SavePlaylist(context, id, std::string(name.data ? name.data : "", name.length),
            count ? std::vector<int64_t>(ids, ids + count) : std::vector<int64_t>{}, completion, data, request);
    });
}

mc_result MC_CALL mc_library_rename_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, mc_string_view name, mc_playlist_write_completion_fn completion, void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id <= 0 || !name.data || !name.length ||
            std::memchr(name.data, '\0', name.length)) return MC_ERROR_INVALID_ARGUMENT;
        const auto reader = library->identity->playlists;
        if (!reader) return MC_ERROR_UNSUPPORTED;
        return reader->RenamePlaylist(context, id, std::string(name.data, name.length), completion, data, request);
    });
}

mc_result MC_CALL mc_library_delete_playlist_async(mc_context *context, const mc_library *library,
    int64_t id, mc_playlist_write_completion_fn completion, void *data, mc_request_id *request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!request) return MC_ERROR_INVALID_ARGUMENT;
        *request = 0;
        if (!context || !library || !completion || id <= 0) return MC_ERROR_INVALID_ARGUMENT;
        const auto reader = library->identity->playlists;
        return reader ? reader->DeletePlaylist(context, id, completion, data, request) : MC_ERROR_UNSUPPORTED;
    });
}

mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context, const mc_library *library, const mc_track_query *query, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !library || !query || !completion || query->struct_size < sizeof(query->struct_size)) {
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
        if (offset > INT64_MAX || limit > limits.max_limit) return MC_ERROR_INVALID_ARGUMENT;
        if (!library->identity->backend) return MC_ERROR_UNSUPPORTED;
        const auto page_limit = limit ? limit : limits.default_limit;

        /* Snapshot inputs before submission. Cancellation leaves worker-owned data alone. */
        const auto identity = library->identity;
        const std::string term(search.length ? search.data : "", search.length);
        std::vector<std::string> names;
        if (!copyFields(fields, count, limits.max_fields, names)) return MC_ERROR_INVALID_ARGUMENT;
        return submitLibraryRequest<mc_track_page>(context,
            [=](std::unique_ptr<mc_track_page> &page) {
                return readPage(*identity, { term, names, offset, page_limit }, page);
            }, completion, user_data, out_request);
    });
}

mc_result MC_CALL mc_library_open_track_search_async(mc_context *context, const mc_library *library,
    const mc_track_search *search,
    mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
        *out_request = 0;
        if (!context || !library || !search || !completion || search->struct_size < sizeof(search->struct_size)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        const auto text = read_option(search, &mc_track_search::text, offsetof(mc_track_search, text));
        const auto mode = read_option(search, &mc_track_search::mode, offsetof(mc_track_search, mode));
        const auto order = read_option(search, &mc_track_search::order, offsetof(mc_track_search, order));
        if (mode > MC_TRACK_SEARCH_REGEX || order > MC_TRACK_SEARCH_GENRE) return MC_ERROR_UNSUPPORTED;
        if (text.length && !text.data) return MC_ERROR_INVALID_ARGUMENT;
        const auto identity = library->identity;
        const std::string term(text.length ? text.data : "", text.length);
        if (identity->async) {
            return identity->async->Search(context, identity,
                { term, mode == MC_TRACK_SEARCH_REGEX, static_cast<query::TrackSortType>(order) },
                completion, user_data, out_request);
        }
        if (!identity->backend) return MC_ERROR_UNSUPPORTED;

        return submitLibraryRequest<mc_track_result>(context,
            [=](std::unique_ptr<mc_track_result> &result) {
                return openSearch(identity, { term, mode == MC_TRACK_SEARCH_REGEX,
                    static_cast<query::TrackSortType>(order) }, result);
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
    return tracks && row < tracks->data->albumStarts.size() && tracks->data->albumStarts[row];
}

uint64_t MC_CALL mc_track_result_get_album_duration(const mc_track_result *tracks, size_t row) {
    if (!tracks) return 0;
    const auto &durations = tracks->data->albumDurations;
    const auto album = durations.find(row);
    return album == durations.end() ? 0 : album->second;
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
        if (limit > limits.max_limit || !copyFields(fields, count, limits.max_fields, names)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        /* Keep the snapshot alive even when the caller releases its handle. */
        const auto data = tracks->data;
        const auto start = std::min(offset, data->ids.size());
        const auto size = std::min(limit ? limit : limits.default_limit, data->ids.size() - start);
        if (size && data->library->async) {
            return data->library->async->ReadPage(context,
                { data->ids.begin() + start, data->ids.begin() + start + size },
                std::move(names), true, completion, user_data, out_request);
        }
        return submitLibraryRequest<mc_track_page>(context,
            [=](std::unique_ptr<mc_track_page> &page) -> mc_result {
                if (size) return readPage(*data->library, { "", names, 0, size, data->ids.data() + start }, page);
                page.reset(new mc_track_page);
                page->names = names;
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

        const auto identity = *track;
        if (identity.library->async) {
            return identity.library->async->ReadPage(context, { identity.id }, std::move(names),
                false, completion, user_data, out_request);
        }
        return submitLibraryRequest<mc_track_page>(context,
            [=](std::unique_ptr<mc_track_page> &page) -> mc_result {
                /* A direct identity lookup distinguishes a removed row from missing tags. */
                const auto status = readPage(*identity.library, { "", names, 0, 1, &identity.id, false }, page);
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
