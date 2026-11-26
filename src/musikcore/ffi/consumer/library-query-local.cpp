#include "library-query-adapter.h"
#include "library-query-executors.h"
#include <musikcore/library/LocalLibrary.h>
#include <common/FilesystemPath.h>
#include <fstream>

namespace musik { namespace core { namespace ffi { namespace detail {
    /* operation borrows a static action label through asynchronous delivery. */
    template<typename Result, typename Command, typename Convert, typename Completion>
    mc_result submitLocal(mc_context* context, const ILibraryPtr& library, const char* operation,
        std::shared_ptr<Command> command, Convert convert, Completion completion, void* data, mc_request_id* request,
        RequestEffect effect = RequestEffect::Read, std::shared_ptr<LibraryScopeState> scopeState = {}) {
        const auto snapshot = scopeState ? scopeState->Capture() : nullptr;
        if (snapshot) {
            command->SetScope(*snapshot);
        }
        const auto scopeRequest = scopeState ? ScopedLibraryRequest::Create(scopeState, snapshot) : nullptr;
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        if (effect == RequestEffect::Mutation) {
            *output = std::make_unique<Result>();
        }
        const auto status = submit_request(
            context,
            [=]() -> mc_result {
                if (scopeState && !scopeState->IsCurrent(snapshot)) {
                    return MC_ERROR_CANCELLED;
                }
                if (library->EnqueueAndWait(command, musik::core::kWaitIndefinite) < 0 ||
                    command->GetStatus() != musik::core::db::IQuery::Finished)
                {
                    return MC_ERROR_IO;
                }
                return convert(*command, *output);
            },
            [=, keepScope = scopeRequest](mc_result status, void* userData) {
                if (scopeState && !scopeState->AdmitCompletion(snapshot)) {
                    status = MC_ERROR_CANCELLED;
                }
                const CompletionError error(status, operation);
                completion(status, status == MC_OK ? output->release() : nullptr, error.Get(), userData);
            },
            data, request, {}, effect);
        if (status == MC_OK && scopeRequest) {
            scopeRequest->Bind(context, *request);
        }
        return status;
    }

    /* runs a read on the local library worker, where it is ordered with catalog
    updates. it borrows the query, result and cancellation flag; the submitting
    call waits for this command before releasing them. */
    template<typename Query, typename Result> class TrackReadCommand final : public query::QueryBase {
    public:
        TrackReadCommand(const query::TrackReadBackend& backend, const Query& query, Result& result,
            const std::atomic_bool& canceled)
        : backend(backend)
        , query(query)
        , result(result)
        , canceled(canceled) {
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

    /* connects a library instance to its local catalog and worker. it keeps the
    library and SQLite reader alive for pending requests and retained results.
    the shared adapter supplies browsing operations; this class handles local
    execution and artwork files. neither copyable nor movable. */
    class LocalTrackReadBackend final : public query::TrackReadBackend,
                                        public PlaybackOrigin,
                                        public LibraryQueryAdapter<LocalTrackReadBackend> {
    public:
        explicit LocalTrackReadBackend(std::shared_ptr<musik::core::library::LocalLibrary> library)
        : library(std::move(library))
        , reader(query::OpenTrackReadBackend(this->library->GetDatabaseFilename())) {}

        bool Run(const query::TrackSearchQuery& query, query::TrackSearchRows& rows,
            const std::atomic_bool& canceled) const override {
            return this->Dispatch(query, rows, canceled);
        }

        bool Run(const query::TrackPageQuery& query, query::ProjectedTrackPage& page,
            const std::atomic_bool& canceled) const override {
            return this->Dispatch(query, page, canceled);
        }

        bool Matches(const musik::core::ILibrary* selected) const override { return selected == this->library.get(); }

        ILibraryPtr Backend() const override { return this->library; }

        mc_result ReadThumbnail(
            int64_t id, std::vector<uint8_t>& bytes, const std::atomic_bool& canceled) const override {
            const auto file = musik::core::sdk::fs::pathFromUtf8(this->library->GetLibraryDirectory()) / "thumbs" /
                (std::to_string(id) + ".jpg");
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
            return stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)) ? MC_OK
                                                                                                          : MC_ERROR_IO;
        }

    private:
        friend class LibraryQueryAdapter<LocalTrackReadBackend>;

        template<typename Result, typename Command, typename Convert, typename Completion>
        mc_result Submit(mc_context* context, const char* operation, std::shared_ptr<Command> command, Convert convert,
            Completion completion, void* data, mc_request_id* request, RequestEffect effect = RequestEffect::Read,
            std::shared_ptr<LibraryScopeState> scopeState = {}) const {
            return submitLocal<Result>(context, this->library, operation, std::move(command), std::move(convert),
                std::move(completion), data, request, effect, std::move(scopeState));
        }

        template<typename Query, typename Result>
        bool Dispatch(const Query& query, Result& result, const std::atomic_bool& canceled) const {
            const auto command =
                std::make_shared<TrackReadCommand<Query, Result>>(*this->reader, query, result, canceled);
            return this->library->EnqueueAndWait(command, musik::core::kWaitIndefinite) >= 0 &&
                command->GetStatus() == musik::core::db::IQuery::Finished;
        }

        const std::shared_ptr<musik::core::library::LocalLibrary> library;
        const std::shared_ptr<const query::TrackReadBackend> reader;
    };


    std::shared_ptr<const LibraryInstance> CreateLocalLibraryInstance(
        const std::shared_ptr<library::LocalLibrary>& library) {
        const auto backend = std::make_shared<LocalTrackReadBackend>(library);
        auto instance = std::make_shared<LibraryInstance>();
        instance->locator = library->GetLibraryDirectory();
        instance->backend = backend;
        instance->playbackOrigin = backend;
        instance->queries = backend;
        return instance;
    }
}}}}
