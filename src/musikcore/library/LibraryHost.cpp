//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include "LibraryHost.h"

#include <musikcore/debug.h>
#include <common/Contract.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/common/primitives.h>
#include <musikcore/ffi/consumer/library-storage.h>
#include <musikcore/ffi/consumer/request.h>
#include <musikcore/ffi/consumer/library-query-backends.h>
#include <musikcore/db/ScopedTransaction.h>
#include <musikcore/library/query/AlbumListQuery.h>
#include <musikcore/library/query/AllCategoriesQuery.h>
#include <musikcore/library/query/AppendPlaylistQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/DeletePlaylistQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/LyricsQuery.h>
#include <musikcore/library/query/MarkTrackPlayedQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/TrackListQueryBase.h>
#include <musikcore/library/query/PlaylistTrackQueries.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/QueryRegistry.h>
#include <nlohmann/json.hpp>
#include <common/runtime/Message.h>
#include <musikcore/support/Messages.h>
#include <musikcore/support/Common.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <string_view>
#include <unordered_set>
#include <chrono>

#define TAG "LibraryHost"

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::library;
using namespace musik::core::runtime;

using PredicateList = musik::core::library::query::category::PredicateList;

/* DATA PROVIDER */

LibraryHost::LibraryHost(mc_context& context, musik::core::ILibraryPtr library,
    musik::core::ffi::PluginRegistry::Factory<mc_playback_remote_factory> factory)
: context(context)
, library(library)
, instance(musik::core::ffi::CreateLibraryInstance(library))
, factory(std::move(factory))
, hostApi {
    .struct_size = sizeof(mc_library_host_api),
    .context = this,
    .search_tracks = SearchTracks,
    .tracks_by_external_ids = TracksByExternalIds,
    .tracks_by_ids = TracksByIds,
    .tracks_by_category = TracksByCategory,
    .tracks_by_categories = TracksByCategories,
    .read_tracks = ReadTracks,
    .read_tracks_async = ReadTracksAsync,
    .run_query = RunQuery,
    .list_category_fields = ListCategoryFields,
    .list_categories = ListCategoriesHost,
    .list_filtered_categories = ListFilteredCategories,
    .list_albums = ListAlbumsHost,
    .read_thumbnail = ReadThumbnail,
    .save_playlist = SavePlaylistHost,
    .rename_playlist = RenamePlaylistHost,
    .append_playlist = AppendPlaylistHost,
    .remove_playlist_entries = RemovePlaylistEntries,
    .delete_playlist = DeletePlaylistHost,
    .read_library_thumbnail = ReadLibraryThumbnail,
} {

}

void LibraryHost::CancelReads() noexcept {
    this->cancelled.store(true);
    {
        std::lock_guard lock(this->admissionMutex);
        this->stopping = true;
    }
    musik::core::ffi::cancel_owned_requests(this->context, this);
}

namespace {
    /* the result owns its library instance, so it remains usable after the
    plugin releases the host service. */
    mc_result trackResult(const std::shared_ptr<const LibraryInstance>& instance,
        const std::shared_ptr<TrackListQueryBase>& query, mc_track_result** out)
    {
        const auto tracks = query->GetResult();
        if (!tracks) {
            return MC_ERROR_IO;
        }
        auto data = std::make_shared<TrackResultData>();
        data->library = instance;
        data->ids = tracks->GetIds();
        data->albumStarts.resize(data->ids.size());
        if (const auto headers = query->GetHeaders()) {
            for (const auto index : *headers) {
                if (index >= data->ids.size()) {
                    return MC_ERROR_IO;
                }
                data->albumStarts[index] = true;
            }
        }
        if (const auto durations = query->GetDurations()) {
            for (const auto& [index, duration] : *durations) {
                if (index >= data->ids.size()) {
                    return MC_ERROR_IO;
                }
                data->albumDurations.emplace(index, duration);
            }
        }
        *out = new mc_track_result { std::move(data) };
        return MC_OK;
    }

    bool validSelections(const mc_library_category_selection* selections, size_t count) {
        if ((count && !selections) || count > PTRDIFF_MAX / sizeof(*selections)) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (!musik::core::ffi::valid_text(selections[i].field, false)) {
                return false;
            }
        }
        return true;
    }

    PredicateList copySelections(const mc_library_category_selection* selections, size_t count) {
        PredicateList result;
        for (size_t i = 0; i < count; ++i) {
            result.emplace_back(std::string(selections[i].field.data, selections[i].field.length), selections[i].id);
        }
        return result;
    }
}

mc_result MC_CALL LibraryHost::SearchTracks(void* context, const mc_track_search* search,
    int32_t limit, int32_t offset, mc_track_result** result)
{
    if (!context || !search || search->struct_size < sizeof(*search) || !result || *result ||
        !musik::core::ffi::valid_text(search->text) || search->mode > MC_TRACK_SEARCH_REGEX ||
        search->order > MC_TRACK_SEARCH_DURATION || limit < -1 || offset < 0)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        const auto match = search->mode == MC_TRACK_SEARCH_REGEX
            ? SearchTrackListQuery::MatchType::Regex : SearchTrackListQuery::MatchType::Substring;
        const std::string filter(search->text.data ? search->text.data : "", search->text.length);
        auto query = std::make_shared<SearchTrackListQuery>(self.library, match, filter,
            static_cast<TrackSortType>(search->order));
        if (limit >= 0) {
            query->SetLimitAndOffset(limit, offset);
        }
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        return trackResult(self.instance, query, result);
    });
}

mc_result MC_CALL LibraryHost::TracksByIds(void* context, const int64_t* ids,
    size_t count, mc_track_result** result)
{
    if (!context || !result || *result || !musik::core::ffi::valid_ids(ids, count)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto data = std::make_shared<TrackResultData>();
        data->library = self.instance;
        if (count) {
            data->ids.assign(ids, ids + count);
        }
        data->albumStarts.resize(count);
        *result = new mc_track_result { std::move(data) };
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::TracksByExternalIds(void* context, const mc_string_view* ids,
    size_t count, mc_track_result** result)
{
    if (!context || !result || *result || (count && !ids) || count > PTRDIFF_MAX / sizeof(*ids)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!musik::core::ffi::valid_text(ids[i], false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }
    if (!count) {
        return TracksByIds(context, nullptr, 0, result);
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        std::vector<std::string> storage;
        std::vector<const char*> values;
        storage.reserve(count);
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            storage.emplace_back(ids[i].data, ids[i].length);
        }
        for (const auto& value : storage) {
            values.push_back(value.c_str());
        }
        auto query = std::make_shared<ExternalIdListToTrackListQuery>(self.library, values.data(), count);
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        return trackResult(self.instance, query, result);
    });
}

mc_result MC_CALL LibraryHost::TracksByCategory(void* context, mc_string_view field, int64_t id,
    mc_string_view filter, int32_t limit, int32_t offset, mc_track_result** result)
{
    if (!context || !result || *result || !musik::core::ffi::valid_text(field) ||
        !musik::core::ffi::valid_text(filter) || id < 0 || limit < -1 || offset < 0)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        const std::string category(field.data ? field.data : "", field.length);
        const std::string term(filter.data ? filter.data : "", filter.length);
        std::shared_ptr<TrackListQueryBase> query;
        if (category == MC_LIBRARY_CATEGORY_PLAYLISTS && id > 0) {
            query = std::make_shared<GetPlaylistQuery>(self.library, id);
        }
        else if (!category.empty() && id > 0) {
            query = std::make_shared<CategoryTrackListQuery>(self.library, category, id, term);
        }
        else {
            query = std::make_shared<CategoryTrackListQuery>(self.library, term);
        }
        if (limit >= 0) {
            query->SetLimitAndOffset(limit, offset);
        }
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        return trackResult(self.instance, query, result);
    });
}

mc_result MC_CALL LibraryHost::TracksByCategories(void* context,
    const mc_library_category_selection* selections, size_t selectionCount,
    mc_string_view filter, mc_track_search_order order, int32_t limit, int32_t offset,
    mc_track_result** result)
{
    if (!context || !result || *result || !validSelections(selections, selectionCount) ||
        !musik::core::ffi::valid_text(filter) || order > MC_TRACK_SEARCH_DURATION || limit < -1 || offset < 0)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto query = std::make_shared<CategoryTrackListQuery>(self.library, copySelections(selections, selectionCount),
            std::string(filter.data ? filter.data : "", filter.length), static_cast<TrackSortType>(order));
        if (limit >= 0) {
            query->SetLimitAndOffset(limit, offset);
        }
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        return trackResult(self.instance, query, result);
    });
}

mc_result MC_CALL LibraryHost::ReadTracks(void* context, const mc_track_result* tracks,
    const mc_track_range* range, mc_track_page** page)
{
    if (!context || !tracks || !tracks->data || !range || range->struct_size < sizeof(*range) ||
        !page || *page || (range->field_count && !range->fields) ||
        range->field_count > PTRDIFF_MAX / sizeof(*range->fields))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    auto& self = *static_cast<LibraryHost*>(context);
    const auto origin = tracks->data->library;
    if (origin != self.instance || !origin || !origin->backend) {
        return MC_ERROR_UNSUPPORTED;
    }
    for (size_t i = 0; i < range->field_count; ++i) {
        if (!musik::core::ffi::valid_text(range->fields[i], false)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        std::vector<std::string> fields;
        if (!range->field_count) {
            fields.emplace_back(MC_TRACK_FIELD_TITLE);
        }
        for (size_t i = 0; i < range->field_count; ++i) {
            fields.emplace_back(range->fields[i].data, range->fields[i].length);
        }
        auto output = std::make_unique<mc_track_page>();
        const auto& ids = tracks->data->ids;
        const size_t offset = std::min(range->offset, ids.size());
        const size_t available = ids.size() - offset;
        const size_t count = range->limit ? std::min(range->limit, available) : available;
        if (count && origin->backend) {
            const std::string search;
            /* explicit IDs can refer to hidden tracks, including direct audio links. */
            const TrackPageQuery query(search, fields, 0, count, ids.data() + offset, true,
                TrackVisibility::IncludeHidden);
            const std::atomic_bool canceled { false };
            if (!origin->backend->Run(query, *output, canceled)) {
                return MC_ERROR_IO;
            }
        }
        else {
            output->names = fields;
        }
        *page = output.release();
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::ReadTracksAsync(void* context, const mc_track_result* tracks,
    const mc_track_range* range, mc_track_query_completion_fn completion, void* userData)
{
    if (!context || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        struct Completion {
            musik::core::ffi::PluginRegistry::Factory<mc_playback_remote_factory> factory;
            mc_track_query_completion_fn callback;
            void* userData;
        };
        auto callback = std::make_unique<Completion>(Completion {
            .factory = self.factory,
            .callback = completion,
            .userData = userData,
        });
        std::lock_guard lock(self.admissionMutex);
        if (self.stopping) {
            return MC_ERROR_CANCELLED;
        }
        mc_request_id request = 0;
        auto* state = callback.release();
        const auto status = musik::core::ffi::read_track_result_scoped(&self.context, tracks, range,
            [](mc_result result, mc_track_page* page, const mc_error* error, void* data) {
                const std::unique_ptr<Completion> state(static_cast<Completion*>(data));
                try {
                    const auto invoked = state->factory.Invoke([&](const auto&) {
                        state->callback(result, page, error, state->userData);
                        return MC_OK;
                    });
                    musik::contract::Assert(invoked == MC_OK, "plugin page completion could not be delivered");
                }
                catch (...) {
                    musik::contract::Assert(false, "plugin page completion threw");
                }
            }, state, &request, &self);
        if (status != MC_OK) {
            delete state;
        }
        return status;
    });
}

mc_result MC_CALL LibraryHost::RunQuery(void* context, mc_string_view queryJson,
    mc_library_text_fn completion, void* userData)
{
    if (!context || !completion || !musik::core::ffi::valid_text(queryJson, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        const std::string input(queryJson.data, queryJson.length);
        const auto parsed = nlohmann::json::parse(input, nullptr, false);
        if (!parsed.is_object() || !parsed.contains("name") || !parsed["name"].is_string() ||
            !parsed.contains("options") ||
            (!parsed["options"].is_object() &&
                !(parsed["name"] == AllCategoriesQuery::kQueryName && parsed["options"].is_null())))
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto& name = parsed["name"].get_ref<const std::string&>();
        const auto& options = parsed["options"];
        if (name == CategoryTrackListQuery::kQueryName || name == SearchTrackListQuery::kQueryName) {
            const auto sort = options.find("sortType");
            if (sort == options.end() || !sort->is_number_integer() ||
                *sort < 0 || *sort > static_cast<int>(TrackSortType::Duration) ||
                !kTrackListSortOrderBy.contains(static_cast<TrackSortType>(sort->get<int>())) )
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
        }
        if (name == "TrackMetadataQuery") {
            constexpr int fullMetadata = 0;
            constexpr int identifiersOnly = 1;
            const auto type = options.find("type");
            if (type == options.end() || !type->is_number_integer() ||
                (*type != fullMetadata && *type != identifiersOnly))
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
        }
        const auto regular = options.find("regularPredicateList");
        if (regular != options.end() && !regular->is_null()) {
            if (!regular->is_array()) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            for (const auto& predicate : *regular) {
                if (!predicate.is_array() || predicate.size() != 2 || !predicate[0].is_string() ||
                    !predicate[1].is_number_integer() ||
                    !category::REGULAR_PROPERTY_MAP.contains(predicate[0].get<std::string>()))
                {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
            }
        }
        if (name == CategoryListQuery::kQueryName || name == CategoryListQuery::kFacetQueryName) {
            const auto field = options.find("trackField");
            const auto type = options.find("outputType");
            if (field == options.end() || !field->is_string() || type == options.end() || !type->is_number_integer()) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            const auto& value = field->get_ref<const std::string&>();
            const int expected = value == "playlists" ? 3 : category::REGULAR_PROPERTY_MAP.contains(value) ? 1 : 2;
            if (*type != expected) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
        }
        auto& self = *static_cast<LibraryHost*>(context);
        std::shared_ptr<musik::core::db::ISerializableQuery> query;
        try {
            query = musik::core::library::QueryRegistry::CreateLocalQueryFor(
                parsed["name"].get<std::string>(), input, self.library);
        }
        catch (const nlohmann::json::exception&) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        catch (const std::invalid_argument&) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (!query) {
            return MC_ERROR_UNSUPPORTED;
        }
        if (self.library->EnqueueAndWait(query) < 0 ||
            query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        const auto output = query->SerializeResult();
        try {
            completion(userData, { .data = output.data(), .length = output.size() });
        }
        catch (...) {
            musik::contract::Assert(false, "library query callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::ListCategoriesHost(void* context, mc_string_view field,
    mc_string_view filter, mc_track_search_mode mode, mc_category_list_completion_fn completion, void* userData)
{
    if (!context || !completion || !musik::core::ffi::valid_text(field, false) ||
        !musik::core::ffi::valid_text(filter, true) ||
        (mode != MC_TRACK_SEARCH_SUBSTRING && mode != MC_TRACK_SEARCH_REGEX))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        const auto match = mode == MC_TRACK_SEARCH_REGEX
            ? CategoryListQuery::MatchType::Regex : CategoryListQuery::MatchType::Substring;
        auto query = std::make_shared<CategoryListQuery>(match,
            std::string(field.data, field.length), std::string(filter.data ? filter.data : "", filter.length));
        self.library->EnqueueAndWait(query);
        if (query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        const auto values = query->GetResult();
        std::vector<std::string> text;
        text.reserve(values->size());
        for (size_t i = 0; i < values->size(); ++i) {
            text.push_back(values->at(i).value);
        }
        std::vector<mc_category_entry> entries;
        entries.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            entries.push_back({
                .id = values->at(i).id,
                .value = { text[i].data(), text[i].size() },
            });
        }
        try {
            completion(MC_OK, entries.data(), entries.size(), nullptr, userData);
        }
        catch (...) {
            musik::contract::Assert(false, "library plugin callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::DeletePlaylistHost(void* context, int64_t playlistId) {
    if (!context || playlistId <= 0) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto query = std::make_shared<DeletePlaylistQuery>(self.library, playlistId);
        self.library->EnqueueAndWait(query);
        return query->GetStatus() == IQuery::Finished ? MC_OK : MC_ERROR_IO;
    });
}

mc_result MC_CALL LibraryHost::ListCategoryFields(void* context,
    mc_category_fields_completion_fn completion, void* userData)
{
    if (!context || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto query = std::make_shared<AllCategoriesQuery>();
        self.library->EnqueueAndWait(query);
        if (query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        const auto fields = query->GetResult();
        std::vector<std::string> values;
        values.reserve(fields->size());
        for (size_t i = 0; i < fields->size(); ++i) {
            values.push_back(fields->at(i).value);
        }
        std::vector<mc_string_view> views;
        views.reserve(values.size());
        for (const auto& value : values) {
            views.push_back({ value.data(), value.size() });
        }
        try {
            completion(MC_OK, views.data(), views.size(), nullptr, userData);
        }
        catch (...) {
            musik::contract::Assert(false, "library plugin callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::ListFilteredCategories(void* context, mc_string_view field,
    const mc_library_category_selection* selections, size_t selectionCount, mc_string_view filter,
    mc_track_search_mode mode, bool facet, mc_string_view trackSearch,
    mc_category_list_completion_fn completion, void* userData)
{
    if (!context || !completion || !musik::core::ffi::valid_text(field, false) ||
        !musik::core::ffi::valid_text(filter) || !musik::core::ffi::valid_text(trackSearch) ||
        mode > MC_TRACK_SEARCH_REGEX || !validSelections(selections, selectionCount))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        const auto match = mode == MC_TRACK_SEARCH_REGEX
            ? CategoryListQuery::MatchType::Regex : CategoryListQuery::MatchType::Substring;
        auto query = std::make_shared<CategoryListQuery>(match, std::string(field.data, field.length),
            copySelections(selections, selectionCount), std::string(filter.data ? filter.data : "", filter.length));
        if (facet) {
            query->SetFacetSearch(std::string(trackSearch.data ? trackSearch.data : "", trackSearch.length));
        }
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        const auto values = query->GetResult();
        std::vector<std::string> text;
        text.reserve(values->size());
        for (size_t i = 0; i < values->size(); ++i) {
            text.push_back(values->at(i).value);
        }
        std::vector<mc_category_entry> entries;
        entries.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            entries.push_back({
                .id = values->at(i).id,
                .value = { .data = text[i].data(), .length = text[i].size() },
            });
        }
        try {
            completion(MC_OK, entries.data(), entries.size(), nullptr, userData);
        }
        catch (...) {
            musik::contract::Assert(false, "library plugin callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::ListAlbumsHost(void* context, mc_string_view filter,
    const mc_library_category_selection* selections, size_t selectionCount,
    mc_library_album_list_fn completion, void* userData)
{
    if (!context || !completion || !musik::core::ffi::valid_text(filter) ||
        !validSelections(selections, selectionCount))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto query = std::make_shared<AlbumListQuery>(copySelections(selections, selectionCount),
            std::string(filter.data ? filter.data : "", filter.length));
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        const auto values = query->GetResult();
        std::vector<std::string> titles, artists;
        titles.reserve(values->size());
        artists.reserve(values->size());
        for (size_t i = 0; i < values->size(); ++i) {
            const auto& album = values->at(i);
            titles.push_back(album.value);
            artists.push_back(AlbumText(album, MC_TRACK_FIELD_ALBUM_ARTIST));
        }
        std::vector<mc_library_album> albums;
        albums.reserve(values->size());
        for (size_t i = 0; i < values->size(); ++i) {
            const auto& album = values->at(i);
            albums.push_back({
                .id = album.id,
                .title = { .data = titles[i].data(), .length = titles[i].size() },
                .artist_id = AlbumInteger(album, MC_TRACK_FIELD_ALBUM_ARTIST_ID),
                .has_artist_id = !AlbumText(album, MC_TRACK_FIELD_ALBUM_ARTIST_ID).empty(),
                .artist = { .data = artists[i].data(), .length = artists[i].size() },
                .thumbnail_id = AlbumInteger(album, MC_TRACK_FIELD_THUMBNAIL_ID),
                .has_thumbnail_id = !AlbumText(album, MC_TRACK_FIELD_THUMBNAIL_ID).empty(),
            });
        }
        try {
            completion(userData, albums.data(), albums.size());
        }
        catch (...) {
            musik::contract::Assert(false, "library plugin callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::ReadThumbnail(void* context, int64_t thumbnailId,
    mc_thumbnail_completion_fn completion, void* userData)
{
    if (!context || thumbnailId <= 0 || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        std::vector<uint8_t> bytes;
        const std::atomic_bool canceled { false };
        const auto status = self.instance->queries->ReadThumbnail(thumbnailId, bytes, canceled);
        if (status != MC_OK) {
            return status;
        }
        try {
            completion(MC_OK, { bytes.data(), bytes.size() }, nullptr, userData);
        }
        catch (...) {
            musik::contract::Assert(false, "library plugin callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::ReadLibraryThumbnail(void* context, const mc_library* library, int64_t thumbnailId,
    mc_thumbnail_completion_fn completion, void* userData)
{
    if (!context || !library || !library->instance || thumbnailId <= 0 || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        if (self.cancelled.load()) {
            return MC_ERROR_CANCELLED;
        }
        if (!library->instance->queries) {
            return MC_ERROR_UNSUPPORTED;
        }
        std::vector<uint8_t> bytes;
        const auto status = library->instance->queries->ReadThumbnail(thumbnailId, bytes, self.cancelled);
        if (self.cancelled.load()) {
            return MC_ERROR_CANCELLED;
        }
        if (status != MC_OK) {
            return status;
        }
        try {
            completion(MC_OK, { bytes.data(), bytes.size() }, nullptr, userData);
        }
        catch (...) {
            musik::contract::Assert(false, "library plugin callback threw");
        }
        return MC_OK;
    });
}

mc_result MC_CALL LibraryHost::SavePlaylistHost(void* context, int64_t playlistId, mc_string_view name,
    const int64_t* trackIds, size_t count, int64_t* savedId)
{
    if (!context || playlistId < 0 || !savedId || !musik::core::ffi::valid_text(name) ||
        (playlistId == 0 && !name.length) || !musik::core::ffi::valid_ids(trackIds, count))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *savedId = 0;
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto tracks = std::make_shared<TrackList>(self.library);
        for (size_t i = 0; i < count; ++i) {
            tracks->Add(trackIds[i]);
        }
        const std::string title(name.data ? name.data : "", name.length);
        auto query = playlistId ? SavePlaylistQuery::Replace(self.library, playlistId, tracks)
            : SavePlaylistQuery::Save(self.library, title, tracks);
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        if (query->ResultCode() != MC_OK) {
            return query->ResultCode();
        }
        if (playlistId && !title.empty()) {
            query = SavePlaylistQuery::Rename(self.library, playlistId, title);
            if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
                return MC_ERROR_IO;
            }
        }
        if (query->ResultCode() != MC_OK) {
            return query->ResultCode();
        }
        *savedId = playlistId ? playlistId : query->GetPlaylistId();
        return *savedId > 0 ? MC_OK : MC_ERROR_IO;
    });
}

mc_result MC_CALL LibraryHost::RenamePlaylistHost(void* context, int64_t playlistId, mc_string_view name) {
    if (!context || playlistId <= 0 || !musik::core::ffi::valid_text(name, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto query = SavePlaylistQuery::Rename(self.library, playlistId, std::string(name.data, name.length));
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        return query->ResultCode();
    });
}

mc_result MC_CALL LibraryHost::AppendPlaylistHost(void* context, int64_t playlistId,
    const int64_t* trackIds, size_t count, int32_t offset)
{
    if (!context || playlistId <= 0 || offset < -1 || !musik::core::ffi::valid_ids(trackIds, count)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        auto tracks = std::make_shared<TrackList>(self.library);
        for (size_t i = 0; i < count; ++i) {
            tracks->Add(trackIds[i]);
        }
        auto query = std::make_shared<AppendPlaylistQuery>(self.library, playlistId, tracks, offset);
        return self.library->EnqueueAndWait(query) >= 0 && query->GetStatus() == IQuery::Finished
            ? MC_OK : MC_ERROR_IO;
    });
}

mc_result MC_CALL LibraryHost::RemovePlaylistEntries(void* context, int64_t playlistId,
    const mc_library_playlist_entry_key* entries, size_t count, size_t* removed)
{
    if (!context || playlistId <= 0 || !removed || (count && !entries) ||
        count > static_cast<size_t>(PTRDIFF_MAX) / sizeof(*entries))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!musik::core::ffi::valid_text(entries[i].external_id, false) || entries[i].position < 0) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }
    *removed = 0;
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        auto& self = *static_cast<LibraryHost*>(context);
        std::vector<std::string> strings;
        std::vector<const char*> ids;
        std::vector<int> positions;
        strings.reserve(count);
        ids.reserve(count);
        positions.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            strings.emplace_back(entries[i].external_id.data, entries[i].external_id.length);
            positions.push_back(entries[i].position);
        }
        for (const auto& value : strings) {
            ids.push_back(value.c_str());
        }
        auto query = std::make_shared<RemoveFromPlaylistQuery>(self.library, playlistId,
            ids.data(), positions.data(), count);
        if (self.library->EnqueueAndWait(query) < 0 || query->GetStatus() != IQuery::Finished) {
            return MC_ERROR_IO;
        }
        *removed = query->GetResult();
        return MC_OK;
    });
}
