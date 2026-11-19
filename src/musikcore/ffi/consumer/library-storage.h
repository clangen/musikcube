#pragma once

#include <musikcore/library.h>
#include <musikcore/library/query/TrackReadBackend.h>

namespace query = musik::core::library::query;
namespace musik { namespace core { class ILibrary; } }

struct LibraryIdentity;

/* Neither copyable nor movable. Identifies an app backend and its connection, if applicable. */
struct PlaybackOrigin {
    PlaybackOrigin() = default;
    virtual ~PlaybackOrigin() = default;
    PlaybackOrigin(const PlaybackOrigin&) = delete;
    PlaybackOrigin& operator=(const PlaybackOrigin&) = delete;

    virtual bool Matches(const musik::core::ILibrary* selected) const = 0;
};

/* Inherits origin ownership. Network reads complete without using the context worker. */
struct AsyncTrackReader : PlaybackOrigin {
    virtual mc_result Search(mc_context *, const std::shared_ptr<const LibraryIdentity> &,
        const query::TrackSearchQuery &, mc_track_search_completion_fn, void *, mc_request_id *) const = 0;
    virtual mc_result ReadPage(mc_context *, std::vector<int64_t>, std::vector<std::string>, bool,
        mc_track_query_completion_fn, void *, mc_request_id *) const = 0;
};

struct mc_track_page : query::ProjectedTrackPage {};

struct LibraryIdentity {
    std::string locator;
    std::shared_ptr<const query::TrackReadBackend> backend;
    std::shared_ptr<const AsyncTrackReader> async;
    std::shared_ptr<const PlaybackOrigin> playbackOrigin;
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
