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
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/library/query/TrackMetadataBatchQuery.h>
#include "library-storage.h"
#include "request.h"
#include "../common/error.h"

#include <unordered_set>

namespace {
    using namespace musik::core::ffi;

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
    class AppTrackReadBackend final : public query::TrackReadBackend {
    public:
        explicit AppTrackReadBackend(std::shared_ptr<musik::core::library::LocalLibrary> library)
            : library(std::move(library)), reader(query::OpenTrackReadBackend(this->library->GetDatabaseFilename())) {}

        bool Run(const query::TrackSearchQuery& query, query::TrackSearchRows& rows) const override {
            return this->Dispatch(query, rows);
        }

        bool Run(const query::TrackPageQuery& query, query::ProjectedTrackPage& page) const override {
            return this->Dispatch(query, page);
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
            completion(result, result == MC_OK ? output->release() : nullptr, nullptr, data);
        }, user_data, out_request, finish);
        if (status != MC_OK) return status;

        /* Backend submission follows admission so even an immediate failure completes on dispatch. */
        const auto submitted = exception_barrier(nullptr, [&]() -> mc_result {
            const bool accepted = library->EnqueueForConnection(command, generation,
                [=](const auto&) {
                    finish(exception_barrier(nullptr, [&]() -> mc_result {
                        if (command->GetStatus() != musik::core::db::IQuery::Finished) return MC_ERROR_IO;
                        return convert(*command, *output);
                    }));
                });
            return accepted ? MC_OK : MC_ERROR_IO;
        });
        if (submitted != MC_OK) finish(submitted);
        return MC_OK;
    }

    mc_result readRemotePage(mc_context *context, const std::shared_ptr<musik::core::library::RemoteLibrary> &library,
        uint64_t generation, std::vector<int64_t> ids,
        std::vector<std::string> fields, bool keepMissing, mc_track_query_completion_fn completion,
        void *user_data, mc_request_id *out_request) {
        for (const auto &field : fields) {
            if (field != MC_TRACK_FIELD_ID && field != MC_TRACK_FIELD_TITLE && field != MC_TRACK_FIELD_ARTIST &&
                field != MC_TRACK_FIELD_ALBUM && field != MC_TRACK_FIELD_DURATION &&
                field != MC_TRACK_FIELD_TRACK && field != MC_TRACK_FIELD_RATING) return MC_ERROR_UNSUPPORTED;
        }

        const auto command = std::make_shared<query::TrackMetadataBatchQuery>(
            std::unordered_set<int64_t>(ids.begin(), ids.end()), library);
        return submitRemote<mc_track_page>(context, library, generation, command,
            [ids = std::move(ids), fields = std::move(fields), keepMissing](auto &batch, auto &output) -> mc_result {
                auto page = std::make_unique<mc_track_page>();
                page->names = fields;
                for (const auto id : ids) {
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

    mc_result openRemoteSearch(mc_context *context, const std::shared_ptr<const LibraryIdentity> &identity,
        const query::TrackSearchQuery &request, const std::shared_ptr<musik::core::library::RemoteLibrary> &library,
        uint64_t generation, mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request) {
        using Match = query::SearchTrackListQuery::MatchType;
        const auto command = std::make_shared<query::SearchTrackListQuery>(library,
            request.regex ? Match::Regex : Match::Substring, request.filter, request.order);
        return submitRemote<mc_track_result>(context, library, generation, command,
            [identity](auto &search, auto &output) {
                auto result = std::make_shared<TrackResultData>();
                result->library = identity;
                const auto rows = search.GetResult();
                for (size_t i = 0; i < rows->Count(); ++i) result->ids.push_back(rows->GetId(i));
                result->albumStarts.resize(result->ids.size());
                for (const auto row : *search.GetHeaders()) {
                    if (row >= result->ids.size()) return MC_ERROR_IO;
                    result->albumStarts[row] = true;
                }
                for (const auto &album : *search.GetDurations()) {
                    if (album.first >= result->ids.size()) return MC_ERROR_IO;
                    result->albumDurations.emplace(album.first, album.second);
                }
                output.reset(new mc_track_result{ std::move(result) });
                return MC_OK;
            }, completion, user_data, out_request);
    }
}

mc_result musik::core::ffi::BindLocalLibrary(const std::shared_ptr<library::LocalLibrary>& library,
    mc_library** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) return MC_ERROR_INVALID_ARGUMENT;
        auto identity = std::make_shared<LibraryIdentity>(LibraryIdentity{
            library->GetLibraryDirectory(), std::make_shared<AppTrackReadBackend>(library), {} });
        *out = new mc_library{ std::move(identity) };
        return MC_OK;
    });
}

mc_result musik::core::ffi::BindRemoteLibrary(const std::shared_ptr<library::RemoteLibrary>& library,
    mc_library** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) return MC_ERROR_INVALID_ARGUMENT;
        const auto generation = library->WebSocketClient().ConnectionGeneration();
        if (!generation) return MC_ERROR_IO;

        auto identity = std::make_shared<LibraryIdentity>();
        identity->async = std::make_shared<AsyncTrackReader>(AsyncTrackReader{
            [library, generation](mc_context *context, const auto &identity, const auto &query,
                auto completion, void *data, mc_request_id *request) {
                return openRemoteSearch(context, identity, query, library, generation, completion, data, request);
            },
            [library, generation](mc_context *context, auto ids, auto fields, bool keepMissing,
                auto completion, void *data, mc_request_id *request) {
                return readRemotePage(context, library, generation, std::move(ids), std::move(fields),
                    keepMissing, completion, data, request);
            }
        });
        *out = new mc_library{ std::move(identity) };
        return MC_OK;
    });
}
