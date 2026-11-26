#include "library-query-adapter.h"
#include "library-query-executors.h"
#include <musikcore/library/RemoteLibraryBackend.h>

namespace musik { namespace core { namespace ffi { namespace detail {
    /* operation borrows a static action label through asynchronous delivery. */
    template<typename Result, typename Command, typename Convert, typename Completion>
    mc_result submitRemote(mc_context* context,
        const std::shared_ptr<musik::core::library::RemoteLibraryBackend>& library, uint64_t generation,
        const char* operation, std::shared_ptr<Command> command, Convert convert, Completion completion,
        void* user_data, mc_request_id* out_request, RequestEffect effect = RequestEffect::Read,
        std::shared_ptr<LibraryScopeState> scopeState = {}, const void* owner = nullptr) {
        const auto snapshot = scopeState ? scopeState->Capture() : nullptr;
        if (snapshot) {
            command->SetScope(*snapshot);
        }
        const auto scopeRequest = scopeState ? ScopedLibraryRequest::Create(scopeState, snapshot) : nullptr;
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        if (effect == RequestEffect::Mutation) {
            *output = std::make_unique<Result>();
        }
        const auto cancelled = (owner || scopeState) ? std::make_shared<std::atomic_bool>(false) : nullptr;
        std::function<void()> cancelExternal;
        if (cancelled) {
            cancelExternal = [library, command] {
                library->DiscardConnectionQuery(command);
            };
        }
        std::function<void(mc_result)> finish;
        const auto status = reserve_external_request(
            context,
            [=, keepScope = scopeRequest](mc_result result, void* data) {
                if (scopeState && !scopeState->AdmitCompletion(snapshot)) {
                    result = MC_ERROR_CANCELLED;
                }
                const CompletionError error(result, operation);
                completion(result, result == MC_OK ? output->release() : nullptr, error.Get(), data);
            },
            user_data, out_request, finish, effect, cancelled, owner, std::move(cancelExternal));
        if (status != MC_OK) {
            return status;
        }

        if (scopeRequest) {
            scopeRequest->Bind(context, *out_request);
        }

        /* Backend submission follows admission so even an immediate failure completes on dispatch. */
        const auto submitted = exception_barrier(nullptr, [&]() -> mc_result {
            const auto admission = library->EnqueueForConnection(command, generation, [=](const auto&) {
                finish(exception_barrier(nullptr, [&]() -> mc_result {
                    if (command->GetStatus() != musik::core::db::IQuery::Finished) {
                        return effect == RequestEffect::Mutation ? MC_ERROR_OUTCOME_UNKNOWN : MC_ERROR_IO;
                    }
                    return convert(*command, *output);
                }));
            });
            /* cancellation can win before transport admission. discard again
            after enqueue so that race cannot leave an unanswered slot behind. */
            if (cancelled && cancelled->load()) {
                library->DiscardConnectionQuery(command);
                return MC_ERROR_CANCELLED;
            }
            using Admission = musik::core::library::RemoteLibraryBackend::ConnectionQueryStatus;
            if (admission == Admission::Busy) {
                return MC_ERROR_BUSY;
            }
            return admission == Admission::Accepted ? MC_OK : MC_ERROR_IO;
        });
        if (submitted != MC_OK) {
            finish(submitted);
        }
        return MC_OK;
    }

    /* connects a library instance to one server connection. queries travel over
    WebSocket and complete on the context dispatcher without blocking its worker.
    it retains the remote library and connection generation so old results cannot
    accidentally read from a replacement connection. neither copyable nor movable. */
    class RemoteLibrary final : public LibraryQueryAdapter<RemoteLibrary> {
    public:
        RemoteLibrary(std::shared_ptr<musik::core::library::RemoteLibraryBackend> library, uint64_t generation)
        : LibraryQueryAdapter("", MC_LIBRARY_REMOTE)
        , library(std::move(library))
        , generation(generation) {}

        bool Matches(const musik::core::LibraryBackend* selected) const override {
            return selected == this->library.get() &&
                this->library->WebSocketClient().ConnectionGeneration() == this->generation;
        }

        LibraryBackend::Shared NativeBackend() const override { return this->library; }


        mc_result Search(mc_context* context, const query::TrackSearchQuery& request,
            mc_track_search_completion_fn completion, void* user_data, mc_request_id* out_request) const override {
            const auto instance = this->shared_from_this();
            using Match = query::SearchTrackListQuery::MatchType;
            const auto command = std::make_shared<query::SearchTrackListQuery>(
                this->library, request.regex ? Match::Regex : Match::Substring, request.filter, request.order);
            return submitRemote<mc_track_result>(
                context, this->library, this->generation, "Search library", command,
                [instance](auto& search, auto& output) {
                    return makeOrderedResult(search, instance, output);
                },
                completion, user_data, out_request, RequestEffect::Read, instance->GetScopeState());
        }


        mc_result ReadThumbnail(
            int64_t id, std::vector<uint8_t>& bytes, const std::atomic_bool& canceled) const override {
            if (!this->Matches(this->library.get()) ||
                !this->library->ReadThumbnail(id, bytes, MaxThumbnailBytes, canceled) ||
                !this->Matches(this->library.get()))
            {
                return MC_ERROR_IO;
            }
            return bytes.empty() ? MC_ERROR_NOT_FOUND : MC_OK;
        }


        mc_result ReadPage(mc_context* context, std::vector<int64_t> ids, std::vector<std::string> fields,
            bool keepMissing, mc_track_query_completion_fn completion, void* user_data, mc_request_id* out_request,
            const void* owner = nullptr) const override {
            if (ids.empty()) {
                /* Empty projections still honor field support and the origin generation. */
                const auto output = std::make_shared<std::unique_ptr<mc_track_page>>(std::make_unique<mc_track_page>());
                (*output)->names = std::move(fields);
                return submit_request(
                    context,
                    [library = this->library, generation = this->generation]() -> mc_result {
                        return library->WebSocketClient().ConnectionGeneration() == generation ? MC_OK : MC_ERROR_IO;
                    },
                    [=](mc_result status, void* data) {
                        const CompletionError error(status, "Read track details");
                        completion(status, status == MC_OK ? output->release() : nullptr, error.Get(), data);
                    },
                    user_data, out_request, {}, RequestEffect::Read, owner);
            }

            const auto command = std::make_shared<query::TrackMetadataBatchQuery>(
                std::unordered_set<int64_t>(ids.begin(), ids.end()), this->library, fields);
            return submitRemote<mc_track_page>(
                context, this->library, this->generation, "Read track details", command,
                [ids = std::move(ids), fields = std::move(fields), keepMissing](
                    auto& batch, auto& output) -> mc_result {
                    auto page = std::make_unique<mc_track_page>();
                    page->names = fields;
                    if (!batch.HasProjectedPage()) {
                        return MC_ERROR_IO;
                    }
                    const auto& rows = batch.Page().rows;
                    for (const auto id : ids) {
                        const auto found =
                            std::lower_bound(rows.begin(), rows.end(), id, [](const auto& row, int64_t value) {
                                return row.id < value;
                            });
                        if (found == rows.end() || found->id != id) {
                            if (!keepMissing) {
                                return MC_ERROR_NOT_FOUND;
                            }
                            page->rows.push_back({ id, std::vector<std::vector<std::string>>(fields.size()) });
                        }
                        else {
                            page->rows.push_back(*found);
                        }
                    }
                    output = std::move(page);
                    return MC_OK;
                },
                completion, user_data, out_request, RequestEffect::Read, {}, owner);
        }

    private:
        friend class LibraryQueryAdapter<RemoteLibrary>;

        template<typename Result, typename Command, typename Convert, typename Completion>
        mc_result Submit(mc_context* context, const char* operation, std::shared_ptr<Command> command, Convert convert,
            Completion completion, void* data, mc_request_id* request, RequestEffect effect = RequestEffect::Read,
            std::shared_ptr<LibraryScopeState> scopeState = {}) const {
            return submitRemote<Result>(context, this->library, this->generation, operation, std::move(command),
                std::move(convert), std::move(completion), data, request, effect, std::move(scopeState));
        }

        const std::shared_ptr<musik::core::library::RemoteLibraryBackend> library;
        const uint64_t generation;
    };
    std::shared_ptr<const musik::core::library::Library> CreateRemoteLibrary(
        const std::shared_ptr<library::RemoteLibraryBackend>& library) {
        const auto generation = library->WebSocketClient().ConnectionGeneration();
        if (!generation) {
            return {};
        }
        return std::make_shared<RemoteLibrary>(library, generation);
    }
} } } }