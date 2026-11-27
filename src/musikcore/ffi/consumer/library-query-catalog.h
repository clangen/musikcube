#pragma once

#include "library-storage.h"

namespace musik { namespace core { namespace ffi { namespace detail {
    /* shared request handling for SQLite reads. the captured instance owns the
    reader until completion, including native reads ordered on the catalog worker. */
    mc_result SubmitCatalogQuery(mc_context* context, musik::core::library::Library::Shared instance,
        const library::query::TrackReadBackend& reader, library::query::TrackPageQuery query,
        mc_track_query_completion_fn completion, void* data, mc_request_id* request);

    mc_result SubmitCatalogSearch(mc_context* context, musik::core::library::Library::Shared instance,
        const library::query::TrackReadBackend& reader, library::query::TrackSearchQuery query,
        mc_track_search_completion_fn completion, void* data, mc_request_id* request);

    mc_result SubmitCatalogPage(mc_context* context, musik::core::library::Library::Shared instance,
        const library::query::TrackReadBackend& reader, std::vector<int64_t> ids, std::vector<std::string> fields,
        bool keepMissing, mc_track_query_completion_fn completion, void* data, mc_request_id* request,
        const void* owner, size_t total);

    mc_result SubmitCatalogThumbnail(mc_context* context, musik::core::library::Library::Shared instance, int64_t id,
        mc_thumbnail_completion_fn completion, void* data, mc_request_id* request);

    musik::core::library::Library::Shared CreateCatalogLibrary(std::string locator, mc_library_kind kind);
} } } }
