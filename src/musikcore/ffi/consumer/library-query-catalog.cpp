#include "library-query-catalog.h"
#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

namespace query = musik::core::library::query;
using namespace musik::core::ffi;

namespace {
    /* action labels have static lifetime so successful requests need no extra
    diagnostic allocation while retaining the label through dispatch. */
    template<typename Result, typename Work, typename Completion>
    mc_result submitLibraryRequest(mc_context* context, const char* operation, Work work, Completion completion,
        void* user_data, mc_request_id* out_request, const void* owner = nullptr,
        std::shared_ptr<LibraryScopeState> scopeState = {},
        std::shared_ptr<const query::LibraryQueryScope> snapshot = {}) {
        const auto scopeRequest = scopeState ? ScopedLibraryRequest::Create(scopeState, snapshot) : nullptr;
        const auto output = std::make_shared<std::unique_ptr<Result>>();
        const auto canceled = std::make_shared<std::atomic_bool>(false);
        const auto status = submit_request(
            context,
            [=]() -> mc_result {
                if (scopeState && !scopeState->IsCurrent(snapshot)) {
                    return MC_ERROR_CANCELLED;
                }
                return work(*output, *canceled);
            },
            [=, keepScope = scopeRequest](mc_result result, void* data) {
                if (scopeState && !scopeState->AdmitCompletion(snapshot)) {
                    result = MC_ERROR_CANCELLED;
                }
                const CompletionError error(result, operation);
                completion(result, result == MC_OK ? output->release() : nullptr, error.Get(), data);
            },
            user_data, out_request, canceled, RequestEffect::Read, owner);
        if (status == MC_OK && scopeRequest) {
            scopeRequest->Bind(context, *out_request);
        }
        return status;
    }

    mc_result readPage(const query::TrackReadBackend& reader, const query::TrackPageQuery& query,
        std::unique_ptr<mc_track_page>& output, const std::atomic_bool& canceled) {
        auto page = std::unique_ptr<mc_track_page>(new mc_track_page);
        if (!reader.Run(query, *page, canceled)) {
            return MC_ERROR_IO;
        }
        for (const auto& row : page->rows) {
            if (row.id <= 0) {
                return MC_ERROR_IO;
            }
        }
        output = std::move(page);
        return MC_OK;
    }

    mc_result openSearch(const std::shared_ptr<const musik::core::library::Library>& library,
        const query::TrackReadBackend& reader, const query::TrackSearchQuery& query,
        std::unique_ptr<mc_track_result>& output, const std::atomic_bool& canceled) {
        auto result = std::make_shared<TrackResultData>();
        if (!reader.Run(query, *result, canceled)) {
            return MC_ERROR_IO;
        }
        if (!valid_ids(result->ids.data(), result->ids.size())) {
            return MC_ERROR_IO;
        }
        result->library = library;
        output.reset(new mc_track_result { std::move(result) });
        return MC_OK;
    }

    /* direct catalog handles only own a SQLite reader. remote placeholders stay
    unsupported until opened through an authenticated remote session. */
    class CatalogLibrary final : public musik::core::library::Library {
    public:
        CatalogLibrary(std::string locator, mc_library_kind kind)
        : musik::core::library::Library(std::move(locator), kind)
        , reader(kind == MC_LIBRARY_LOCAL ? query::OpenTrackReadBackend(this->Locator() + "/musik.db") : nullptr) {}

        mc_result QueryTracks(mc_context* context, const query::TrackPageQuery& query,
            mc_track_query_completion_fn completion, void* data, mc_request_id* request) const override {
            return this->reader ? detail::SubmitCatalogQuery(context, this->shared_from_this(), *this->reader, query,
                                      completion, data, request)
                                : MC_ERROR_UNSUPPORTED;
        }

        mc_result Search(mc_context* context, const query::TrackSearchQuery& query,
            mc_track_search_completion_fn completion, void* data, mc_request_id* request) const override {
            return this->reader ? detail::SubmitCatalogSearch(context, this->shared_from_this(), *this->reader, query,
                                      completion, data, request)
                                : MC_ERROR_UNSUPPORTED;
        }

        mc_result ReadPage(mc_context* context, std::vector<int64_t> ids, std::vector<std::string> fields,
            bool keepMissing, mc_track_query_completion_fn completion, void* data, mc_request_id* request,
            const void* owner) const override {
            return this->reader ? detail::SubmitCatalogPage(context, this->shared_from_this(), *this->reader,
                                      std::move(ids), std::move(fields), keepMissing, completion, data, request, owner)
                                : MC_ERROR_UNSUPPORTED;
        }

    private:
        const std::shared_ptr<const query::TrackReadBackend> reader;
    };
}

namespace musik { namespace core { namespace ffi { namespace detail {
    mc_result SubmitCatalogQuery(mc_context* context, musik::core::library::Library::Shared instance,
        const query::TrackReadBackend& reader, query::TrackPageQuery query, mc_track_query_completion_fn completion,
        void* data, mc_request_id* request) {
        const auto scope = instance->GetScopeState();
        const auto snapshot = scope->Capture();
        return submitLibraryRequest<mc_track_page>(
            context, "Read track details",
            [instance, &reader, search = std::string(query.search), fields = std::vector<std::string>(query.fields),
                offset = query.offset, limit = query.limit, snapshot](auto& page, const auto& cancelled) {
                query::TrackPageQuery command(search, fields, offset, limit);
                command.scope = *snapshot;
                return readPage(reader, command, page, cancelled);
            },
            completion, data, request, nullptr, scope, snapshot);
    }

    mc_result SubmitCatalogSearch(mc_context* context, musik::core::library::Library::Shared instance,
        const query::TrackReadBackend& reader, query::TrackSearchQuery query, mc_track_search_completion_fn completion,
        void* data, mc_request_id* request) {
        const auto scope = instance->GetScopeState();
        const auto snapshot = scope->Capture();
        query.scope = *snapshot;
        return submitLibraryRequest<mc_track_result>(
            context, "Search library",
            [instance, &reader, query = std::move(query)](auto& result, const auto& cancelled) {
                return openSearch(instance, reader, query, result, cancelled);
            },
            completion, data, request, nullptr, scope, snapshot);
    }

    mc_result SubmitCatalogPage(mc_context* context, musik::core::library::Library::Shared instance,
        const query::TrackReadBackend& reader, std::vector<int64_t> ids, std::vector<std::string> fields,
        bool keepMissing, mc_track_query_completion_fn completion, void* data, mc_request_id* request,
        const void* owner) {
        return submitLibraryRequest<mc_track_page>(
            context, "Read track details",
            [instance, &reader, ids = std::move(ids), fields = std::move(fields), keepMissing](
                auto& page, const auto& cancelled) -> mc_result {
                if (ids.empty()) {
                    page = std::make_unique<mc_track_page>();
                    page->names = fields;
                    return MC_OK;
                }
                const auto status =
                    readPage(reader, { "", fields, 0, ids.size(), ids.data(), keepMissing }, page, cancelled);
                if (status != MC_OK) {
                    return status;
                }
                return !keepMissing && page->rows.empty() ? MC_ERROR_NOT_FOUND : MC_OK;
            },
            completion, data, request, owner);
    }

    mc_result SubmitCatalogThumbnail(mc_context* context, musik::core::library::Library::Shared instance, int64_t id,
        mc_thumbnail_completion_fn completion, void* data, mc_request_id* request) {
        using Image = std::vector<uint8_t>;
        const auto output = std::make_shared<Image>();
        const auto stop = std::make_shared<std::stop_source>();
        return submit_request(context,
            [instance, id, output, stop] { return instance->ReadThumbnail(id, *output, stop->get_token()); },
            [completion, output](mc_result status, void* userData) {
                const CompletionError error(status, "Load album artwork");
                completion(status, status == MC_OK ? mc_byte_view { output->data(), output->size() } : mc_byte_view {},
                    error.Get(), userData);
            }, data, request, {}, RequestEffect::Read, nullptr, [stop] { stop->request_stop(); });
    }

    musik::core::library::Library::Shared CreateCatalogLibrary(std::string locator, mc_library_kind kind) {
        return std::make_shared<CatalogLibrary>(std::move(locator), kind);
    }
} } } }
