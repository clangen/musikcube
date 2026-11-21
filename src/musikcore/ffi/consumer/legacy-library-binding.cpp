/*
 * Adapt the app's existing LocalLibrary and RemoteLibrary instances to the
 * consumer ABI. During migration, the app still creates and selects libraries
 * through MasterLibrary. The ABI uses those same instances to run typed queries
 * and deliver completions through its context.
 *
 * Each handle retains the backend selected at creation. A search can finish
 * after a library switch, and a retained reference to track 42 must still
 * resolve in its original library. Remote handles also capture the connection
 * generation so a reconnect cannot redirect a read from an old reference.
 *
 * Origin binding is already implemented for search and metadata reads. The
 * loopback test musikcore.abi.remote_query_transport exercises cancellation,
 * library switching, and rejection of an old track reference after reconnect.
 * Remove this bridge once core creates and selects ABI-native library handles
 * whose backends own query dispatch and connection lifetimes.
 */

#include "library-binding.h"
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/DirectoryTrackListQuery.h>
#include <musikcore/library/query/CategoryListQuery.h>
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

#include <unordered_set>

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

    using CategoryFields = std::vector<std::string>;

    mc_result collectFields(query::AllCategoriesQuery& command, std::unique_ptr<CategoryFields>& output) {
        output = std::make_unique<CategoryFields>();
        const auto result = command.GetResult();
        output->reserve(result->Count());
        for (size_t i = 0; i < result->Count(); ++i) {
            output->push_back(result->At(i)->ToString());
        }
        return MC_OK;
    }

    auto deliverFields(mc_category_fields_completion_fn completion) {
        return [completion](mc_result status, CategoryFields* fields, const mc_error* error, void* data) {
            const std::unique_ptr<CategoryFields> owned(fields);
            std::vector<mc_string_view> views;
            if (fields) {
                views.reserve(fields->size());
                for (const auto& field : *fields) {
                    views.push_back({ field.data(), field.size() });
                }
            }
            completion(status, views.data(), views.size(), error, data);
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
        output = std::make_unique<int64_t>(std::max(int64_t{0}, command.GetPlaylistId()));
        return MC_OK;
    }

    auto collectDeletedPlaylistId(int64_t id) {
        return [id](query::DeletePlaylistQuery&, std::unique_ptr<int64_t>& output) {
            output = std::make_unique<int64_t>(id);
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
        std::shared_ptr<Command> command, Convert convert, Completion completion, void* data, mc_request_id* request) {
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        return submit_request(context, [=]() -> mc_result {
            if (library->EnqueueAndWait(command, musik::core::kWaitIndefinite) < 0 ||
                command->GetStatus() != musik::core::db::IQuery::Finished) return MC_ERROR_IO;
            return convert(*command, *output);
        }, [=](mc_result status, void* userData) {
            const CompletionError error(status);
            completion(status, status == MC_OK ? output->release() : nullptr, error.Get(), userData);
        }, data, request);
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
        TrackReadCommand(const query::TrackReadBackend& backend, const Query& query, Result& result)
            : backend(backend), query(query), result(result) {
            this->SetOptions(SuppressCompletionBroadcast);
        }

        std::string Name() override { return "TrackReadCommand"; }

    private:
        bool OnRun(musik::core::db::Connection&) override {
            return this->backend.Run(this->query, this->result);
        }

        const query::TrackReadBackend& backend;
        const Query& query;
        Result& result;
    };

    /* Inherits noncopyable and nonmovable ownership. */
    class AppTrackReadBackend final : public query::TrackReadBackend, public PlaybackOrigin, public LibraryQueries {
    public:
        explicit AppTrackReadBackend(std::shared_ptr<musik::core::library::LocalLibrary> library)
            : library(std::move(library)), reader(query::OpenTrackReadBackend(this->library->GetDatabaseFilename())) {}

        bool Run(const query::TrackSearchQuery& query, query::TrackSearchRows& rows) const override {
            return this->Dispatch(query, rows);
        }

        bool Run(const query::TrackPageQuery& query, query::ProjectedTrackPage& page) const override {
            return this->Dispatch(query, page);
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

        mc_result ListPlaylists(mc_context* context, mc_playlist_list_completion_fn completion,
            void* data, mc_request_id* request) const override {
            return submitLocal<PlaylistEntries>(context, this->library, listPlaylistsCommand(),
                collectEntries<PlaylistEntries>, deliverEntries<PlaylistEntries>(completion), data, request);
        }

        mc_result SavePlaylist(mc_context* context, int64_t id, std::string name, std::vector<int64_t> ids, bool append,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<int64_t>(context, this->library, savePlaylistCommand(this->library, id, name, ids, append),
                collectPlaylistId, deliverPlaylistWrite(completion), data, request);
        }

        mc_result RenamePlaylist(mc_context* context, int64_t id, std::string name,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<int64_t>(context, this->library, query::SavePlaylistQuery::Rename(this->library, id, name),
                collectPlaylistId, deliverPlaylistWrite(completion), data, request);
        }

        mc_result DeletePlaylist(mc_context* context, int64_t id,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitLocal<int64_t>(context, this->library, std::make_shared<query::DeletePlaylistQuery>(this->library, id),
                collectDeletedPlaylistId(id), deliverPlaylistWrite(completion), data, request);
        }

        mc_result SetTrackRating(mc_context* context, int64_t id, uint32_t rating,
            mc_library_completion_fn completion, void* data, mc_request_id* request) const override
        {
            return submitLocal<bool>(context, this->library,
                std::make_shared<query::SetTrackRatingQuery>(id, static_cast<int>(rating)),
                commandSucceeded, deliverStatus(completion), data, request);
        }

    private:
        template<typename Query, typename Result>
        bool Dispatch(const Query& query, Result& result) const {
            const auto command = std::make_shared<TrackReadCommand<Query, Result>>(*this->reader, query, result);
            return this->library->EnqueueAndWait(command, musik::core::kWaitIndefinite) >= 0 &&
                command->GetStatus() == musik::core::db::IQuery::Finished;
        }

        const std::shared_ptr<musik::core::library::LocalLibrary> library;
        const std::shared_ptr<const query::TrackReadBackend> reader;
    };

    template<typename Result, typename Command, typename Convert, typename Completion>
    mc_result submitRemote(mc_context *context, const std::shared_ptr<musik::core::library::RemoteLibrary> &library,
        uint64_t generation, std::shared_ptr<Command> command, Convert convert, Completion completion,
        void *user_data, mc_request_id *out_request) {
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        std::function<void(mc_result)> finish;
        const auto status = reserve_external_request(context, [=](mc_result result, void *data) {
            const CompletionError error(result);
            completion(result, result == MC_OK ? output->release() : nullptr, error.Get(), data);
        }, user_data, out_request, finish);
        if (status != MC_OK) return status;

        /* Backend submission follows admission so even an immediate failure completes on dispatch. */
        const auto submitted = exception_barrier(nullptr, [&]() -> mc_result {
            const auto admission = library->EnqueueForConnection(command, generation,
                [=](const auto&) {
                    finish(exception_barrier(nullptr, [&]() -> mc_result {
                        if (command->GetStatus() != musik::core::db::IQuery::Finished) return MC_ERROR_IO;
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
    class RemoteTrackReader final : public AsyncTrackReader, public LibraryQueries {
    public:
        RemoteTrackReader(std::shared_ptr<musik::core::library::RemoteLibrary> library, uint64_t generation)
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

        mc_result ListPlaylists(mc_context* context, mc_playlist_list_completion_fn completion,
            void* data, mc_request_id* request) const override {
            return submitRemote<PlaylistEntries>(context, this->library, this->generation, listPlaylistsCommand(),
                collectEntries<PlaylistEntries>, deliverEntries<PlaylistEntries>(completion), data, request);
        }

        mc_result SavePlaylist(mc_context* context, int64_t id, std::string name, std::vector<int64_t> ids, bool append,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitRemote<int64_t>(context, this->library, this->generation,
                savePlaylistCommand(this->library, id, name, ids, append), collectPlaylistId,
                deliverPlaylistWrite(completion), data, request);
        }

        mc_result RenamePlaylist(mc_context* context, int64_t id, std::string name,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitRemote<int64_t>(context, this->library, this->generation,
                query::SavePlaylistQuery::Rename(this->library, id, name),
                collectPlaylistId, deliverPlaylistWrite(completion), data, request);
        }

        mc_result DeletePlaylist(mc_context* context, int64_t id,
            mc_playlist_write_completion_fn completion, void* data, mc_request_id* request) const override {
            return submitRemote<int64_t>(context, this->library, this->generation,
                std::make_shared<query::DeletePlaylistQuery>(this->library, id),
                collectDeletedPlaylistId(id), deliverPlaylistWrite(completion), data, request);
        }

        mc_result SetTrackRating(mc_context* context, int64_t id, uint32_t rating,
            mc_library_completion_fn completion, void* data, mc_request_id* request) const override
        {
            return submitRemote<bool>(context, this->library, this->generation,
                std::make_shared<query::SetTrackRatingQuery>(id, static_cast<int>(rating)),
                commandSucceeded, deliverStatus(completion), data, request);
        }

        mc_result ReadPage(mc_context *context, std::vector<int64_t> ids,
            std::vector<std::string> fields, bool keepMissing, mc_track_query_completion_fn completion,
            void *user_data, mc_request_id *out_request) const override {
            for (const auto &field : fields) {
                if (field != MC_TRACK_FIELD_ID && field != MC_TRACK_FIELD_TITLE && field != MC_TRACK_FIELD_ARTIST &&
                    field != MC_TRACK_FIELD_ALBUM && field != MC_TRACK_FIELD_DURATION &&
                    field != MC_TRACK_FIELD_TRACK && field != MC_TRACK_FIELD_RATING) return MC_ERROR_UNSUPPORTED;
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
        const auto backend = std::make_shared<AppTrackReadBackend>(library);
        return std::make_shared<LibraryIdentity>(LibraryIdentity {
            library->GetLibraryDirectory(), backend, {}, backend, backend });
    }
    if (const auto library = std::dynamic_pointer_cast<library::RemoteLibrary>(selected)) {
        const auto generation = library->WebSocketClient().ConnectionGeneration();
        if (!generation) {
            return {};
        }

        auto identity = std::make_shared<LibraryIdentity>();
        const auto reader = std::make_shared<RemoteTrackReader>(library, generation);
        identity->async = reader;
        identity->playbackOrigin = reader;
        identity->queries = reader;
        identity->kind = MC_LIBRARY_REMOTE;
        return identity;
    }
    return {};
}

mc_result musik::core::ffi::BindLocalLibrary(const std::shared_ptr<library::LocalLibrary>& library,
    mc_library** out)
{
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        *out = new mc_library { CaptureLibrary(library) };
        return MC_OK;
    });
}

mc_result musik::core::ffi::BindRemoteLibrary(const std::shared_ptr<library::RemoteLibrary>& library,
    mc_library** out)
{
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
