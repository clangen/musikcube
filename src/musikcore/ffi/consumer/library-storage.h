#pragma once

#include <musikcore/library.h>
#include <musikcore/library/query/TrackReadBackend.h>

#include <functional>

namespace query = musik::core::library::query;

struct LibraryIdentity;

/* Owned by the library identity. Network reads complete without using the context worker. */
struct AsyncTrackReader {
    std::function<mc_result(mc_context *, const std::shared_ptr<const LibraryIdentity> &,
        const query::TrackSearchQuery &, mc_track_search_completion_fn, void *, mc_request_id *)> search;
    std::function<mc_result(mc_context *, std::vector<int64_t>, std::vector<std::string>, bool,
        mc_track_query_completion_fn, void *, mc_request_id *)> page;
};

struct mc_track_page : query::ProjectedTrackPage {};

struct LibraryIdentity {
    std::string locator;
    std::shared_ptr<const query::TrackReadBackend> backend;
    std::shared_ptr<const AsyncTrackReader> async;
};

struct mc_library {
    std::shared_ptr<const LibraryIdentity> identity;
};

struct TrackResultData : query::TrackSearchRows {
    std::shared_ptr<const LibraryIdentity> library;
};

struct mc_track_result {
    std::shared_ptr<const TrackResultData> data;
};

struct mc_track_ref {
    std::shared_ptr<const LibraryIdentity> library;
    int64_t id;
};
