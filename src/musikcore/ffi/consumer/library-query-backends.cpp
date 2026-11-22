/* Typed library queries retain the catalog and, for remote reads, the
   connection generation. A retained search result or track reference can
   therefore survive selection changes without resolving in a new catalog. */

#include "library-query-backends.h"
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
#include <musikcore/library/query/AlbumListQuery.h>
#include <musikcore/library/query/AllCategoriesQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/DeletePlaylistQuery.h>
#include <musikcore/library/query/SetTrackRatingQuery.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>
#include "library-storage.h"
#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace query = musik::core::library::query;

namespace {
    using namespace musik::core::ffi;
    using musik::core::ILibraryPtr;

    template<typename Entry>
    struct LibraryEntries {
        std::vector<std::string> names;
        std::vector<Entry> entries;
    };

    using PlaylistEntries = LibraryEntries<mc_playlist_entry>;
    using CategoryEntries = LibraryEntries<mc_category_entry>;
    constexpr size_t MaxThumbnailBytes = 8 * 1024 * 1024;

    struct AlbumEntries {
        std::vector<std::string> titles;
        std::vector<std::string> artists;
        std::vector<mc_album_entry> entries;
    };

    mc_result collectAlbums(query::AlbumListQuery& command, std::unique_ptr<AlbumEntries>& output) {
        auto albums = std::make_unique<AlbumEntries>();
        const auto result = command.GetResult();
        albums->titles.reserve(result->Count());
        albums->artists.reserve(result->Count());
        albums->entries.reserve(result->Count());
        for (size_t i = 0; i < result->Count(); ++i) {
            const auto entry = result->GetSharedAt(i);
            const auto& title = albums->titles.emplace_back(entry->GetTypeValue());
            const auto& artist = albums->artists.emplace_back(entry->Get(MC_TRACK_FIELD_ALBUM_ARTIST));
            albums->entries.push_back({ entry->GetId(), { title.data(), title.size() },
                { artist.data(), artist.size() }, entry->GetInt64(MC_TRACK_FIELD_THUMBNAIL_ID) });
        }
        output = std::move(albums);
        return MC_OK;
    }

    auto deliverStatus(mc_library_completion_fn completion) {
        return [completion](mc_result result, bool*, const mc_error* error, void* data) {
            completion(result, error, data);
        };
    }

    mc_result commandSucceeded(const query::QueryBase&, std::unique_ptr<bool>&) {
        return MC_OK;
    }

    auto listPlaylistsCommand() {
        return std::make_shared<query::CategoryListQuery>(query::CategoryListQuery::MatchType::Substring,
            musik::core::library::constants::Playlists::TABLE_NAME);
    }

    template<typename Entries>
    mc_result collectEntries(query::CategoryListQuery& command, std::unique_ptr<Entries>& output) {
        auto list = std::make_unique<Entries>();
        const auto result = command.GetResult();
        list->names.reserve(result->Count());
        list->entries.reserve(result->Count());
        for (size_t i = 0; i < result->Count(); ++i) {
            const auto entry = result->At(i);
            const auto& name = list->names.emplace_back(entry->ToString());
            list->entries.push_back({entry->GetId(), {name.data(), name.size()}});
        }
        output = std::move(list);
        return MC_OK;
    }

    template<typename Entries, typename Completion>
    auto deliverEntries(Completion completion) {
        return [completion](mc_result status, Entries* list, const mc_error* error, void* data) {
            const std::unique_ptr<Entries> owned(list);
            completion(status, list ? list->entries.data() : nullptr, list ? list->entries.size() : 0, error, data);
        };
    }

    struct CategoryFields {
        std::vector<std::string> names;
        std::vector<mc_string_view> views;
    };

    mc_result collectFields(query::AllCategoriesQuery& command, std::unique_ptr<CategoryFields>& output) {
        output = std::make_unique<CategoryFields>();
        const auto result = command.GetResult();
        output->names.reserve(result->Count());
        output->views.reserve(result->Count());
        for (size_t i = 0; i < result->Count(); ++i) {
            const auto& name = output->names.emplace_back(result->At(i)->ToString());
            output->views.push_back({ name.data(), name.size() });
        }
        return MC_OK;
    }

    auto deliverFields(mc_category_fields_completion_fn completion) {
        return [completion](mc_result status, CategoryFields* fields, const mc_error* error, void* data) {
            const std::unique_ptr<CategoryFields> owned(fields);
            completion(status, fields ? fields->views.data() : nullptr,
                fields ? fields->views.size() : 0, error, data);
        };
    }

    auto savePlaylistCommand(const ILibraryPtr& library, int64_t id, const std::string& name,
        const std::vector<int64_t>& ids, bool append) {
        const auto tracks = std::make_shared<musik::core::TrackList>(library, ids.data(), ids.size());
        if (append) return query::SavePlaylistQuery::Append(library, id, tracks);
        return id ? query::SavePlaylistQuery::Replace(library, id, tracks)
                  : query::SavePlaylistQuery::Save(library, name, tracks);
    }

    mc_result collectPlaylistId(query::SavePlaylistQuery& command, std::unique_ptr<int64_t>& output) {
        *output = std::max(int64_t { 0 }, command.GetPlaylistId());
        return MC_OK;
    }

    auto collectDeletedPlaylistId(int64_t id) {
        return [id](query::DeletePlaylistQuery&, std::unique_ptr<int64_t>& output) {
            *output = id;
            return MC_OK;
        };
    }

    auto deliverPlaylistWrite(mc_playlist_write_completion_fn completion) {
        return [completion](mc_result status, int64_t* id, const mc_error* error, void* data) {
            const std::unique_ptr<int64_t> owned(id);
            completion(status, id ? *id : 0, error, data);
        };
    }

    template<typename Result, typename Command, typename Convert, typename Completion>
    mc_result submitLocal(mc_context* context, const ILibraryPtr& library,
        std::shared_ptr<Command> command, Convert convert, Completion completion, void* data, mc_request_id* request,
        RequestEffect effect = RequestEffect::Read) {
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        if (effect == RequestEffect::Mutation) {
            *output = std::make_unique<Result>();
        }
        return submit_request(context, [=]() -> mc_result {
            if (library->EnqueueAndWait(command, musik::core::kWaitIndefinite) < 0 ||
                command->GetStatus() != musik::core::db::IQuery::Finished) return MC_ERROR_IO;
            return convert(*command, *output);
        }, [=](mc_result status, void* userData) {
            const CompletionError error(status);
            completion(status, status == MC_OK ? output->release() : nullptr, error.Get(), userData);
        }, data, request, {}, effect);
    }

    mc_result makeOrderedResult(query::TrackListQueryBase& command,
        const std::shared_ptr<const LibraryIdentity>& identity, std::unique_ptr<mc_track_result>& output) {
        /* Some legacy queries return fresh shared owners from their accessors. */
        /* Retain each owner before borrowing its storage for conversion. */
        const auto rows = command.GetResult();
        const auto headers = command.GetHeaders();
        const auto durations = command.GetDurations();
        if (!rows || !headers || !durations) return MC_ERROR_IO;

        auto result = std::make_shared<TrackResultData>();
        result->library = identity;
        result->ids = rows->GetIds();
        if (!valid_ids(result->ids.data(), result->ids.size())) return MC_ERROR_IO;
        result->albumStarts.resize(result->ids.size());
        for (const auto row : *headers) {
            if (row >= result->ids.size()) return MC_ERROR_IO;
            result->albumStarts[row] = true;
        }
        for (const auto& album : *durations) {
            if (album.first >= result->ids.size()) return MC_ERROR_IO;
            result->albumDurations.emplace(album.first, album.second);
        }
        output.reset(new mc_track_result{ std::move(result) });
        return MC_OK;
    }

    std::shared_ptr<query::TrackListQueryBase> selectionCommand(const ILibraryPtr& library,
        const std::string& category, const std::string& directory, int64_t id, query::TrackSortType order) {
        if (category.empty()) return std::make_shared<query::DirectoryTrackListQuery>(library, directory);
        /* The typed query binds extended metadata keys as values. */
        return std::make_shared<query::CategoryTrackListQuery>(library, category, id, "", order);
    }

    /* Synchronous commands borrow their inputs until the library dispatcher returns. */
    template<typename Query, typename Result>
    class TrackReadCommand final : public query::QueryBase {
    public:
        TrackReadCommand(const query::TrackReadBackend& backend, const Query& query, Result& result,
            const std::atomic_bool& canceled)
            : backend(backend), query(query), result(result), canceled(canceled) {
            this->SetOptions(SuppressCompletionBroadcast);
        }

        std::string Name() override { return "TrackReadCommand"; }

    private:
        bool OnRun(musik::core::db::Connection&) override {
            return this->backend.Run(this->query, this->result, this->canceled);
        }

        const query::TrackReadBackend& backend;
        const Query& query;
        Result& result;
        const std::atomic_bool& canceled;
    };

    /* Inherits noncopyable and nonmovable ownership. */
    class LocalTrackReadBackend final : public query::TrackReadBackend, public PlaybackOrigin, public LibraryQueries {
    public:
        explicit LocalTrackReadBackend(std::shared_ptr<musik::core::library::LocalLibrary> library)
            : library(std::move(library)), reader(query::OpenTrackReadBackend(this->library->GetDatabaseFilename())) {}

        bool Run(const query::TrackSearchQuery& query, query::TrackSearchRows& rows,
            const std::atomic_bool& canceled) const override {
            return this->Dispatch(query, rows, canceled);
        }

        bool Run(const query::TrackPageQuery& query, query::ProjectedTrackPage& page,
            const std::atomic_bool& canceled) const override {
            return this->Dispatch(query, page, canceled);
        }

        bool Matches(const musik::core::ILibrary* selected) const override {
            return selected == this->library.get();
        }

        ILibraryPtr Backend() const override { return this->library; }

        mc_result OpenPlaylist(mc_context* context, const std::shared_ptr<const LibraryIdentity>& identity,
            int64_t id, mc_track_search_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<mc_track_result>(context, this->library,
                std::make_shared<query::GetPlaylistQuery>(this->library, id),
                [identity](auto& command, auto& output) {
                    return makeOrderedResult(command, identity, output);
                }, completion, data, request);
        }

        mc_result OpenSelection(mc_context* context, const std::shared_ptr<const LibraryIdentity>& identity,
            std::string category, std::string directory, int64_t id, query::TrackSortType order,
            mc_track_search_completion_fn completion, void* data, mc_request_id* request) const override {
            const auto command = selectionCommand(this->library, category, directory, id, order);
            return submitLocal<mc_track_result>(context, this->library, command,
                [identity](auto& query, auto& output) { return makeOrderedResult(query, identity, output); },
                completion, data, request);
        }

        mc_result ListCategoryFields(mc_context* context, mc_category_fields_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            return submitLocal<CategoryFields>(context, this->library,
                std::make_shared<query::AllCategoriesQuery>(), collectFields, deliverFields(completion), data, request);
        }

        mc_result ListCategories(mc_context* context, std::string field, std::string filter,
            mc_track_search_mode mode, mc_category_list_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            using MatchType = query::CategoryListQuery::MatchType;
            const auto match = mode == MC_TRACK_SEARCH_REGEX ? MatchType::Regex : MatchType::Substring;
            const auto command = std::make_shared<query::CategoryListQuery>(match, field, filter);
            return submitLocal<CategoryEntries>(context, this->library, command,
                collectEntries<CategoryEntries>, deliverEntries<CategoryEntries>(completion), data, request);
        }

        mc_result ListFacets(mc_context* context, std::string field, std::string search,
            query::category::PredicateList selections, mc_category_list_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            auto command = std::make_shared<query::CategoryListQuery>(
                query::CategoryListQuery::MatchType::Substring, field, std::move(selections));
            command->SetFacetSearch(std::move(search));
            return submitLocal<CategoryEntries>(context, this->library, command,
                collectEntries<CategoryEntries>, deliverEntries<CategoryEntries>(completion), data, request);
        }

        mc_result ListAlbums(mc_context* context, std::string filter, mc_album_list_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            return submitLocal<AlbumEntries>(context, this->library,
                std::make_shared<query::AlbumListQuery>(filter), collectAlbums,
                deliverEntries<AlbumEntries>(completion), data, request);
        }

        mc_result ReadThumbnail(int64_t id, std::vector<uint8_t>& bytes,
            const std::atomic_bool& canceled) const override
        {
            const auto file = std::filesystem::u8path(this->library->GetLibraryDirectory()) /
                "thumbs" / (std::to_string(id) + ".jpg");
            std::ifstream stream(file, std::ios::binary | std::ios::ate);
            if (!stream) {
                return MC_ERROR_NOT_FOUND;
            }
            const auto size = stream.tellg();
            if (size <= 0 || size > static_cast<std::streamoff>(MaxThumbnailBytes)) {
                return MC_ERROR_IO;
            }
            if (canceled.load()) {
                return MC_ERROR_CANCELLED;
            }
            bytes.resize(static_cast<size_t>(size));
            stream.seekg(0);
            return stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))
                ? MC_OK
                : MC_ERROR_IO;
        }

        mc_result ListPlaylists(mc_context* context, mc_playlist_list_completion_fn completion,
            void* data, mc_request_id* request) const override {
            return submitLocal<PlaylistEntries>(context, this->library, listPlaylistsCommand(),
                collectEntries<PlaylistEntries>, deliverEntries<PlaylistEntries>(completion), data, request);
        }

        mc_result SavePlaylist(mc_context* context, int64_t id, std::string name, std::vector<int64_t> ids, bool append,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<int64_t>(context, this->library, savePlaylistCommand(this->library, id, name, ids, append),
                collectPlaylistId, deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result RenamePlaylist(mc_context* context, int64_t id, std::string name,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<int64_t>(context, this->library, query::SavePlaylistQuery::Rename(this->library, id, name),
                collectPlaylistId, deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result DeletePlaylist(mc_context* context, int64_t id,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<int64_t>(context, this->library, std::make_shared<query::DeletePlaylistQuery>(this->library, id),
                collectDeletedPlaylistId(id), deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result SetTrackRating(mc_context* context, int64_t id, uint32_t rating,
            mc_library_completion_fn completion, void* data, mc_request_id* request) const override
        {
            return submitLocal<bool>(context, this->library,
                std::make_shared<query::SetTrackRatingQuery>(id, static_cast<int>(rating)),
                commandSucceeded, deliverStatus(completion), data, request, RequestEffect::Mutation);
        }

    private:
        template<typename Query, typename Result>
        bool Dispatch(const Query& query, Result& result, const std::atomic_bool& canceled) const {
            const auto command = std::make_shared<TrackReadCommand<Query, Result>>(*this->reader, query, result, canceled);
            return this->library->EnqueueAndWait(command, musik::core::kWaitIndefinite) >= 0 &&
                command->GetStatus() == musik::core::db::IQuery::Finished;
        }

        const std::shared_ptr<musik::core::library::LocalLibrary> library;
        const std::shared_ptr<const query::TrackReadBackend> reader;
    };

    template<typename Result, typename Command, typename Convert, typename Completion>
    mc_result submitRemote(mc_context *context, const std::shared_ptr<musik::core::library::RemoteLibrary> &library,
        uint64_t generation, std::shared_ptr<Command> command, Convert convert, Completion completion,
        void *user_data, mc_request_id *out_request, RequestEffect effect = RequestEffect::Read) {
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        if (effect == RequestEffect::Mutation) {
            *output = std::make_unique<Result>();
        }
        std::function<void(mc_result)> finish;
        const auto status = reserve_external_request(context, [=](mc_result result, void *data) {
            const CompletionError error(result);
            completion(result, result == MC_OK ? output->release() : nullptr, error.Get(), data);
        }, user_data, out_request, finish, effect);
        if (status != MC_OK) return status;

        /* Backend submission follows admission so even an immediate failure completes on dispatch. */
        const auto submitted = exception_barrier(nullptr, [&]() -> mc_result {
            const auto admission = library->EnqueueForConnection(command, generation,
                [=](const auto&) {
                    finish(exception_barrier(nullptr, [&]() -> mc_result {
                        if (command->GetStatus() != musik::core::db::IQuery::Finished) {
                            return effect == RequestEffect::Mutation ? MC_ERROR_OUTCOME_UNKNOWN : MC_ERROR_IO;
                        }
                        return convert(*command, *output);
                    }));
                });
            using Admission = musik::core::library::RemoteLibrary::ConnectionQueryStatus;
            if (admission == Admission::Busy) return MC_ERROR_BUSY;
            return admission == Admission::Accepted ? MC_OK : MC_ERROR_IO;
        });
        if (submitted != MC_OK) finish(submitted);
        return MC_OK;
    }

    /* Neither copyable nor movable. Retains one remote library and connection generation. */
    class RemoteTrackReadBackend final : public AsyncTrackReader, public LibraryQueries {
    public:
        RemoteTrackReadBackend(std::shared_ptr<musik::core::library::RemoteLibrary> library, uint64_t generation)
            : library(std::move(library)), generation(generation) {}

        bool Matches(const musik::core::ILibrary* selected) const override {
            return selected == this->library.get() &&
                this->library->WebSocketClient().ConnectionGeneration() == this->generation;
        }

        ILibraryPtr Backend() const override { return this->library; }

        mc_result OpenPlaylist(mc_context* context, const std::shared_ptr<const LibraryIdentity>& identity,
            int64_t id, mc_track_search_completion_fn completion, void* data, mc_request_id* request) const override {
            const auto command = std::make_shared<query::GetPlaylistQuery>(this->library, id);
            return submitRemote<mc_track_result>(context, this->library, this->generation, command,
                [identity](auto& playlist, auto& output) {
                    return makeOrderedResult(playlist, identity, output);
                }, completion, data, request);
        }

        mc_result OpenSelection(mc_context* context, const std::shared_ptr<const LibraryIdentity>& identity,
            std::string category, std::string directory, int64_t id, query::TrackSortType order,
            mc_track_search_completion_fn completion, void* data, mc_request_id* request) const override {
            const auto command = selectionCommand(this->library, category, directory, id, order);
            return submitRemote<mc_track_result>(context, this->library, this->generation, command,
                [identity](auto& query, auto& output) { return makeOrderedResult(query, identity, output); },
                completion, data, request);
        }

        mc_result Search(mc_context *context, const std::shared_ptr<const LibraryIdentity> &identity,
            const query::TrackSearchQuery &request, mc_track_search_completion_fn completion,
            void *user_data, mc_request_id *out_request) const override {
            using Match = query::SearchTrackListQuery::MatchType;
            const auto command = std::make_shared<query::SearchTrackListQuery>(this->library,
                request.regex ? Match::Regex : Match::Substring, request.filter, request.order);
            return submitRemote<mc_track_result>(context, this->library, this->generation, command,
                [identity](auto &search, auto &output) {
                    return makeOrderedResult(search, identity, output);
                }, completion, user_data, out_request);
        }

        mc_result ListCategoryFields(mc_context* context, mc_category_fields_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            return submitRemote<CategoryFields>(context, this->library, this->generation,
                std::make_shared<query::AllCategoriesQuery>(), collectFields, deliverFields(completion), data, request);
        }

        mc_result ListCategories(mc_context* context, std::string field, std::string filter,
            mc_track_search_mode mode, mc_category_list_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            using MatchType = query::CategoryListQuery::MatchType;
            const auto match = mode == MC_TRACK_SEARCH_REGEX ? MatchType::Regex : MatchType::Substring;
            const auto command = std::make_shared<query::CategoryListQuery>(match, field, filter);
            return submitRemote<CategoryEntries>(context, this->library, this->generation, command,
                collectEntries<CategoryEntries>, deliverEntries<CategoryEntries>(completion), data, request);
        }

        mc_result ListFacets(mc_context* context, std::string field, std::string search,
            query::category::PredicateList selections, mc_category_list_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            auto command = std::make_shared<query::CategoryListQuery>(
                query::CategoryListQuery::MatchType::Substring, field, std::move(selections));
            command->SetFacetSearch(std::move(search));
            return submitRemote<CategoryEntries>(context, this->library, this->generation, command,
                collectEntries<CategoryEntries>, deliverEntries<CategoryEntries>(completion), data, request);
        }

        mc_result ListAlbums(mc_context* context, std::string filter, mc_album_list_completion_fn completion,
            void* data, mc_request_id* request) const override
        {
            return submitRemote<AlbumEntries>(context, this->library, this->generation,
                std::make_shared<query::AlbumListQuery>(filter), collectAlbums,
                deliverEntries<AlbumEntries>(completion), data, request);
        }

        mc_result ReadThumbnail(int64_t id, std::vector<uint8_t>& bytes,
            const std::atomic_bool& canceled) const override
        {
            if (!this->Matches(this->library.get()) ||
                !this->library->ReadThumbnail(id, bytes, MaxThumbnailBytes, canceled) ||
                !this->Matches(this->library.get()))
            {
                return MC_ERROR_IO;
            }
            return bytes.empty() ? MC_ERROR_NOT_FOUND : MC_OK;
        }

        mc_result ListPlaylists(mc_context* context, mc_playlist_list_completion_fn completion,
            void* data, mc_request_id* request) const override {
            return submitRemote<PlaylistEntries>(context, this->library, this->generation, listPlaylistsCommand(),
                collectEntries<PlaylistEntries>, deliverEntries<PlaylistEntries>(completion), data, request);
        }

        mc_result SavePlaylist(mc_context* context, int64_t id, std::string name, std::vector<int64_t> ids, bool append,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitRemote<int64_t>(context, this->library, this->generation,
                savePlaylistCommand(this->library, id, name, ids, append), collectPlaylistId,
                deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result RenamePlaylist(mc_context* context, int64_t id, std::string name,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitRemote<int64_t>(context, this->library, this->generation,
                query::SavePlaylistQuery::Rename(this->library, id, name),
                collectPlaylistId, deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result DeletePlaylist(mc_context* context, int64_t id,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitRemote<int64_t>(context, this->library, this->generation,
                std::make_shared<query::DeletePlaylistQuery>(this->library, id),
                collectDeletedPlaylistId(id), deliverPlaylistWrite(completion), data, request, RequestEffect::Mutation);
        }

        mc_result SetTrackRating(mc_context* context, int64_t id, uint32_t rating,
            mc_library_completion_fn completion, void* data, mc_request_id* request) const override
        {
            return submitRemote<bool>(context, this->library, this->generation,
                std::make_shared<query::SetTrackRatingQuery>(id, static_cast<int>(rating)),
                commandSucceeded, deliverStatus(completion), data, request, RequestEffect::Mutation);
        }

        mc_result ReadPage(mc_context *context, std::vector<int64_t> ids,
            std::vector<std::string> fields, bool keepMissing, mc_track_query_completion_fn completion,
            void *user_data, mc_request_id *out_request) const override {
            for (const auto &field : fields) {
                if (field != MC_TRACK_FIELD_ID && field != MC_TRACK_FIELD_TITLE && field != MC_TRACK_FIELD_ARTIST &&
                    field != MC_TRACK_FIELD_ALBUM && field != MC_TRACK_FIELD_DURATION &&
                    field != MC_TRACK_FIELD_TRACK && field != MC_TRACK_FIELD_RATING &&
                    field != MC_TRACK_FIELD_LYRICS) return MC_ERROR_UNSUPPORTED;
            }

            if (ids.empty()) {
                /* Empty projections still honor field support and the origin generation. */
                const auto output = std::make_shared<std::unique_ptr<mc_track_page>>(std::make_unique<mc_track_page>());
                (*output)->names = std::move(fields);
                return submit_request(context, [library = this->library, generation = this->generation]() -> mc_result {
                    return library->WebSocketClient().ConnectionGeneration() == generation ? MC_OK : MC_ERROR_IO;
                }, [=](mc_result status, void* data) {
                    const CompletionError error(status);
                    completion(status, status == MC_OK ? output->release() : nullptr, error.Get(), data);
                }, user_data, out_request);
            }

            const auto command = std::make_shared<query::TrackMetadataBatchQuery>(
                std::unordered_set<int64_t>(ids.begin(), ids.end()), this->library, fields);
            return submitRemote<mc_track_page>(context, this->library, this->generation, command,
                [ids = std::move(ids), fields = std::move(fields), keepMissing](auto &batch, auto &output) -> mc_result {
                    auto page = std::make_unique<mc_track_page>();
                    page->names = fields;
                    for (const auto id : ids) {
                        if (batch.HasProjectedPage()) {
                            const auto& rows = batch.Page().rows;
                            const auto found = std::lower_bound(rows.begin(), rows.end(), id,
                                [](const auto& row, int64_t value) { return row.id < value; });
                            if (found == rows.end() || found->id != id) {
                                if (!keepMissing) return MC_ERROR_NOT_FOUND;
                                page->rows.push_back({id, std::vector<std::vector<std::string>>(fields.size())});
                            }
                            else page->rows.push_back(*found);
                            continue;
                        }
                        const auto track = batch.Result().find(id);
                        if (track == batch.Result().end() && !keepMissing) return MC_ERROR_NOT_FOUND;

                        /* The server returns an ID map. Restore the snapshot order and missing slots. */
                        query::ProjectedTrackPage::Row row{ id, std::vector<std::vector<std::string>>(fields.size()) };
                        if (track != batch.Result().end()) {
                            for (size_t i = 0; i < fields.size(); ++i) {
                                row.fields[i].push_back(fields[i] == MC_TRACK_FIELD_ID
                                    ? std::to_string(id) : track->second->GetString(fields[i].c_str()));
                            }
                        }
                        page->rows.push_back(std::move(row));
                    }
                    output = std::move(page);
                    return MC_OK;
                }, completion, user_data, out_request);
        }

    private:
        const std::shared_ptr<musik::core::library::RemoteLibrary> library;
        const uint64_t generation;
    };
}

std::shared_ptr<const LibraryIdentity> musik::core::ffi::CaptureLibrary(const ILibraryPtr& selected) {
    if (const auto library = std::dynamic_pointer_cast<library::LocalLibrary>(selected)) {
        const auto backend = std::make_shared<LocalTrackReadBackend>(library);
        return std::make_shared<LibraryIdentity>(LibraryIdentity {
            library->GetLibraryDirectory(), backend, {}, backend, backend });
    }
    if (const auto library = std::dynamic_pointer_cast<library::RemoteLibrary>(selected)) {
        const auto generation = library->WebSocketClient().ConnectionGeneration();
        if (!generation) {
            return {};
        }

        auto identity = std::make_shared<LibraryIdentity>();
        const auto reader = std::make_shared<RemoteTrackReadBackend>(library, generation);
        identity->async = reader;
        identity->playbackOrigin = reader;
        identity->queries = reader;
        identity->kind = MC_LIBRARY_REMOTE;
        return identity;
    }
    return {};
}

mc_result musik::core::ffi::OpenLibraryHandle(const ILibraryPtr& library, mc_library** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        auto identity = CaptureLibrary(library);
        if (!identity) {
            return MC_ERROR_IO;
        }
        *out = new mc_library { std::move(identity) };
        return MC_OK;
    });
}

musik::core::ffi::LibraryStatus musik::core::ffi::CaptureLibraryStatus(const ILibraryPtr& selected) {
    LibraryStatus status;
    using ConnectionState = ILibrary::ConnectionState;
    status.kind = selected->GetType() == ILibrary::Type::Local ? MC_LIBRARY_LOCAL : MC_LIBRARY_REMOTE;
    status.configured = selected->IsConfigured();
    switch (selected->GetConnectionState()) {
        case ConnectionState::Disconnected:
            status.state = MC_LIBRARY_DISCONNECTED;
            break;
        case ConnectionState::Connecting:
            status.state = MC_LIBRARY_CONNECTING;
            break;
        case ConnectionState::Connected:
            status.state = MC_LIBRARY_CONNECTED;
            break;
        case ConnectionState::AuthenticationFailure:
            status.state = MC_LIBRARY_AUTHENTICATION_FAILED;
            break;
    }
    if (const auto remote = std::dynamic_pointer_cast<library::RemoteLibrary>(selected)) {
        const auto& socket = remote->WebSocketClient();
        status.endpoint = socket.Uri();
        status.serverVersion = socket.LastServerVersion();
        using Error = net::WebSocketClient::ConnectionError;
        switch (socket.LastConnectionError()) {
            case Error::None:
                break;
            case Error::ClosedByServer:
                status.error = MC_LIBRARY_CONNECTION_CLOSED;
                break;
            case Error::ConnectionFailed:
                status.error = MC_LIBRARY_CONNECTION_FAILED;
                break;
            case Error::InvalidPassword:
                status.error = MC_LIBRARY_CONNECTION_INVALID_PASSWORD;
                break;
            case Error::IncompatibleVersion:
                status.error = MC_LIBRARY_CONNECTION_INCOMPATIBLE_VERSION;
                break;
        }
    }

    return status;
}
