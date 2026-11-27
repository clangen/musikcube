#pragma once

#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/AlbumListQuery.h>
#include <musikcore/library/query/SourceListQuery.h>
#include <musikcore/library/query/AllCategoriesQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/DeletePlaylistQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>
#include "library-storage.h"
#include "library-query-catalog.h"
#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <mutex>
#include <unordered_set>


namespace musik { namespace core { namespace ffi { namespace detail {
    namespace query = musik::core::library::query;

    using namespace musik::core::ffi;
    using musik::core::LibraryBackend;

    /* keeps list labels alive while their borrowed C views are delivered to
    the caller. the completed request owns this storage until its callback ends. */
    template<typename Entry> struct LibraryEntries {
        std::vector<std::string> names;
        std::vector<Entry> entries;
    };

    using SourceEntries = LibraryEntries<mc_library_source>;

    inline mc_result collectSources(query::SourceListQuery& command, std::unique_ptr<SourceEntries>& output) {
        auto list = std::make_unique<SourceEntries>();
        const auto& rows = command.GetResult();
        list->names.reserve(rows.size());
        list->entries.reserve(rows.size());
        for (const auto& row : rows) {
            const auto& label = list->names.emplace_back(row.label);
            list->entries.push_back({ row.id, { label.data(), label.size() } });
        }
        output = std::move(list);
        return MC_OK;
    }

    using PlaylistEntries = LibraryEntries<mc_playlist_entry>;
    using CategoryEntries = LibraryEntries<mc_category_entry>;
    struct CategoryPageEntries : CategoryEntries {
        size_t total = 0;
        size_t anchorIndex = SIZE_MAX;
    };
    constexpr size_t MaxThumbnailBytes = 8 * 1024 * 1024;

    /* owns album titles and artist names behind the C callback's borrowed views.
    converting the whole result first keeps those strings stable during delivery. */
    struct AlbumEntries {
        std::vector<std::string> titles;
        std::vector<std::string> artists;
        std::vector<mc_album_entry> entries;
    };

    inline mc_result collectAlbums(query::AlbumListQuery& command, std::unique_ptr<AlbumEntries>& output) {
        auto albums = std::make_unique<AlbumEntries>();
        const auto result = command.GetResult();
        albums->titles.reserve(result->size());
        albums->artists.reserve(result->size());
        albums->entries.reserve(result->size());
        for (size_t i = 0; i < result->size(); ++i) {
            const auto& entry = result->at(i);
            const auto& title = albums->titles.emplace_back(entry.value);
            const auto& artist = albums->artists.emplace_back(query::AlbumText(entry, MC_TRACK_FIELD_ALBUM_ARTIST));
            albums->entries.push_back({ entry.id, { title.data(), title.size() }, { artist.data(), artist.size() },
                query::AlbumInteger(entry, MC_TRACK_FIELD_THUMBNAIL_ID) });
        }
        output = std::move(albums);
        return MC_OK;
    }

    inline auto deliverStatus(mc_library_completion_fn completion) {
        return [completion](mc_result result, bool*, const mc_error* error, void* data) {
            completion(result, error, data);
        };
    }

    inline mc_result commandSucceeded(const query::QueryBase&, std::unique_ptr<bool>&) {
        return MC_OK;
    }

    inline auto listPlaylistsCommand() {
        return std::make_shared<query::CategoryListQuery>(
            query::CategoryListQuery::MatchType::Substring, musik::core::library::constants::Playlists::TABLE_NAME);
    }

    template<typename Entries>
    inline mc_result collectEntries(query::CategoryListQuery& command, std::unique_ptr<Entries>& output) {
        auto list = std::make_unique<Entries>();
        const auto result = command.GetResult();
        list->names.reserve(result->size());
        list->entries.reserve(result->size());
        for (size_t i = 0; i < result->size(); ++i) {
            const auto& entry = result->at(i);
            const auto& name = list->names.emplace_back(entry.value);
            if constexpr (std::is_same_v<Entries, PlaylistEntries>) {
                list->entries.push_back({ entry.id, { name.data(), name.size() },
                    static_cast<uint32_t>(entry.sourceId ? MC_PLAYLIST_SOURCE : MC_PLAYLIST_GLOBAL),
                    entry.sourceId.value_or(0) });
            }
            else {
                list->entries.push_back({ entry.id, { name.data(), name.size() } });
            }
        }
        output = std::move(list);
        return MC_OK;
    }

    template<typename Entries, typename Completion> inline auto deliverEntries(Completion completion) {
        return [completion](mc_result status, Entries* list, const mc_error* error, void* data) {
            const std::unique_ptr<Entries> owned(list);
            completion(status, list ? list->entries.data() : nullptr, list ? list->entries.size() : 0, error, data);
        };
    }

    /* owns the field names returned by category discovery until the caller's
    callback has finished reading its array of string views. */
    struct CategoryFields {
        std::vector<std::string> names;
        std::vector<mc_string_view> views;
    };

    inline mc_result collectFields(query::AllCategoriesQuery& command, std::unique_ptr<CategoryFields>& output) {
        output = std::make_unique<CategoryFields>();
        const auto result = command.GetResult();
        output->names.reserve(result->size());
        output->views.reserve(result->size());
        for (size_t i = 0; i < result->size(); ++i) {
            const auto& name = output->names.emplace_back(result->at(i).value);
            output->views.push_back({ name.data(), name.size() });
        }
        return MC_OK;
    }

    inline auto deliverFields(mc_category_fields_completion_fn completion) {
        return [completion](mc_result status, CategoryFields* fields, const mc_error* error, void* data) {
            const std::unique_ptr<CategoryFields> owned(fields);
            completion(status, fields ? fields->views.data() : nullptr, fields ? fields->views.size() : 0, error, data);
        };
    }

    inline auto savePlaylistCommand(const LibraryBackend::Shared& library, int64_t id, const std::string& name,
        const std::vector<int64_t>& ids, bool append, std::optional<int32_t> owner = {}) {
        const auto tracks = std::make_shared<musik::core::TrackList>(library, ids.data(), ids.size());
        if (append) {
            return query::SavePlaylistQuery::Append(library, id, tracks);
        }
        return id ? query::SavePlaylistQuery::Replace(library, id, tracks)
                  : query::SavePlaylistQuery::CreateOwned(library, name, tracks, owner);
    }

    inline mc_result collectPlaylistId(query::SavePlaylistQuery& command, std::unique_ptr<int64_t>& output) {
        if (command.ResultCode() != MC_OK) {
            return command.ResultCode();
        }
        *output = std::max(int64_t { 0 }, command.GetPlaylistId());
        return MC_OK;
    }

    inline auto collectDeletedPlaylistId(int64_t id) {
        return [id](query::DeletePlaylistQuery&, std::unique_ptr<int64_t>& output) {
            *output = id;
            return MC_OK;
        };
    }

    inline auto deliverPlaylistWrite(mc_playlist_write_completion_fn completion) {
        return [completion](mc_result status, int64_t* id, const mc_error* error, void* data) {
            const std::unique_ptr<int64_t> owned(id);
            completion(status, id ? *id : 0, error, data);
        };
    }

    inline mc_result makeOrderedResult(query::TrackListQueryBase& command,
        const std::shared_ptr<const musik::core::library::Library>& instance,
        std::unique_ptr<mc_track_result>& output) {
        /* Some legacy queries return fresh shared owners from their accessors. */
        /* Retain each owner before borrowing its storage for conversion. */
        const auto rows = command.GetResult();
        const auto headers = command.GetHeaders();
        const auto durations = command.GetDurations();
        if (!rows || !headers || !durations) {
            return MC_ERROR_IO;
        }

        auto result = std::make_shared<TrackResultData>();
        result->library = instance;
        result->ids = rows->GetIds();
        if (!valid_ids(result->ids.data(), result->ids.size())) {
            return MC_ERROR_IO;
        }
        result->albumStarts.resize(result->ids.size());
        for (const auto row : *headers) {
            if (row >= result->ids.size()) {
                return MC_ERROR_IO;
            }
            result->albumStarts[row] = true;
        }
        for (const auto& album : *durations) {
            if (album.first >= result->ids.size()) {
                return MC_ERROR_IO;
            }
            result->albumDurations.emplace(album.first, album.second);
        }
        output.reset(new mc_track_result { std::move(result) });
        return MC_OK;
    }

    inline std::shared_ptr<query::TrackListQueryBase> selectionCommand(const LibraryBackend::Shared& library,
        const std::string& category, const std::string& directory, int64_t id, query::TrackSortType order) {
        if (category.empty()) {
            return std::make_shared<query::DirectoryTrackListQuery>(library, directory);
        }
        /* The typed query binds extended metadata keys as values. */
        return std::make_shared<query::CategoryTrackListQuery>(library, category, id, "", order);
    }

    /* implements the browsing and editing operations shared by local and remote
    libraries. it builds typed queries and converts their results for C callers;
    the selected executor supplies scheduling and keeps the backend alive.
    keeping these operations together gives both backends the same scope rules. */
    template<typename Executor> class LibraryQueryAdapter : public musik::core::library::Library {
    public:
        using musik::core::library::Library::Library;

        mc_result ReadThumbnailAsync(mc_context* context, int64_t id, mc_thumbnail_completion_fn completion, void* data,
            mc_request_id* request) const override {
            return SubmitCatalogThumbnail(context, this->shared_from_this(), id, completion, data, request);
        }

        mc_result OpenPlaylist(mc_context* context, int64_t id, mc_track_search_completion_fn completion, void* data,
            mc_request_id* request) const override {
            const auto instance = this->shared_from_this();
            return this->Self().template Submit<mc_track_result>(
                context, "Open playlist", std::make_shared<query::GetPlaylistQuery>(this->Self().library, id),
                [instance](auto& command, auto& output) {
                    return makeOrderedResult(command, instance, output);
                },
                completion, data, request);
        }

        mc_result OpenSelection(mc_context* context, std::string category, std::string directory, int64_t id,
            query::TrackSortType order, mc_track_search_completion_fn completion, void* data,
            mc_request_id* request) const override {
            const auto instance = this->shared_from_this();
            const auto command = selectionCommand(this->Self().library, category, directory, id, order);
            return this->Self().template Submit<mc_track_result>(
                context, category.empty() ? "Open folder" : "Open category", command,
                [instance](auto& query, auto& output) {
                    return makeOrderedResult(query, instance, output);
                },
                completion, data, request, RequestEffect::Read, instance->GetScopeState());
        }

        mc_result ListCategoryFields(mc_context* context, mc_category_fields_completion_fn completion, void* data,
            mc_request_id* request) const override {
            return this->Self().template Submit<CategoryFields>(context, "List category types",
                std::make_shared<query::AllCategoriesQuery>(), collectFields, deliverFields(completion), data, request);
        }

        mc_result QueryCategories(mc_context* context, std::string field, std::string filter, mc_track_search_mode mode,
            std::string search, mc_track_search_mode trackMode, query::category::PredicateList selections,
            int64_t anchor, size_t offset, size_t limit, mc_category_page_completion_fn completion, void* data,
            mc_request_id* request) const override {
            const auto scopeState = this->GetScopeState();
            const auto match = mode == MC_TRACK_SEARCH_REGEX ? query::CategoryListQuery::MatchType::Regex
                                                             : query::CategoryListQuery::MatchType::Substring;
            auto command = std::make_shared<query::CategoryListQuery>(match, field, selections, filter);
            if (!search.empty() || !selections.empty()) {
                command->SetTrackFilter(std::move(search),
                    trackMode == MC_TRACK_SEARCH_REGEX ? query::CategoryListQuery::MatchType::Regex
                                                       : query::CategoryListQuery::MatchType::Substring);
            }
            command->SetPage(anchor, offset, limit);
            return this->Self().template Submit<CategoryPageEntries>(
                context, "List category page", command,
                [](auto& query, std::unique_ptr<CategoryPageEntries>& output) {
                    auto status = collectEntries<CategoryPageEntries>(query, output);
                    if (status == MC_OK) {
                        output->total = query.GetTotal();
                        output->anchorIndex = query.GetAnchorIndex();
                    }
                    return status;
                },
                [completion](mc_result status, CategoryPageEntries* page, const mc_error* error, void* user) {
                    std::unique_ptr<CategoryPageEntries> owned(page);
                    const mc_category_page view { page ? page->entries.data() : nullptr,
                        page ? page->entries.size() : 0, page ? page->total : 0, page ? page->anchorIndex : SIZE_MAX };
                    completion(status, page ? &view : nullptr, error, user);
                },
                data, request, RequestEffect::Read, scopeState);
        }

        mc_result ListAlbums(mc_context* context, std::string filter, mc_album_list_completion_fn completion,
            void* data, mc_request_id* request) const override {
            const auto scopeState = this->GetScopeState();
            return this->Self().template Submit<AlbumEntries>(context, "List albums",
                std::make_shared<query::AlbumListQuery>(filter), collectAlbums,
                deliverEntries<AlbumEntries>(completion), data, request, RequestEffect::Read, scopeState);
        }

        mc_result ListSources(mc_context* context, mc_library_sources_completion_fn completion, void* data,
            mc_request_id* request) const override {
            return this->Self().template Submit<SourceEntries>(context, "List sources",
                std::make_shared<query::SourceListQuery>(this->Self().library), collectSources,
                deliverEntries<SourceEntries>(completion), data, request);
        }

        mc_result ListPlaylists(mc_context* context, mc_playlist_list_completion_fn completion, void* data,
            mc_request_id* request) const override {
            const auto scopeState = this->GetScopeState();
            return this->Self().template Submit<PlaylistEntries>(context, "List playlists", listPlaylistsCommand(),
                collectEntries<PlaylistEntries>, deliverEntries<PlaylistEntries>(completion), data, request,
                RequestEffect::Read, scopeState);
        }

        mc_result SavePlaylist(mc_context* context, int64_t id, std::string name, std::vector<int64_t> ids, bool append,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request,
            std::optional<int32_t> owner = {}) const override {
            return this->Self().template Submit<int64_t>(context, append ? "Add tracks to playlist" : "Save playlist",
                savePlaylistCommand(this->Self().library, id, name, ids, append, owner), collectPlaylistId,
                deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result RenamePlaylist(mc_context* context, int64_t id, std::string name,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return this->Self().template Submit<int64_t>(context, "Rename playlist",
                query::SavePlaylistQuery::Rename(this->Self().library, id, name), collectPlaylistId,
                deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result DeletePlaylist(mc_context* context, int64_t id, mc_playlist_write_completion_fn completion,
            void* data, mc_request_id* request) const override {
            return this->Self().template Submit<int64_t>(context, "Delete playlist",
                std::make_shared<query::DeletePlaylistQuery>(this->Self().library, id), collectDeletedPlaylistId(id),
                deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result SetTrackRating(mc_context* context, int64_t id, uint32_t rating, mc_library_completion_fn completion,
            void* data, mc_request_id* request) const override {
            return this->Self().template Submit<bool>(context, "Change track rating",
                std::make_shared<query::SetTrackRatingQuery>(id, static_cast<int>(rating)), commandSucceeded,
                deliverStatus(completion), data, request, RequestEffect::Mutation);
        }

    private:
        const Executor& Self() const { return static_cast<const Executor&>(*this); }
    };


} } } }
