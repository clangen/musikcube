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

/* Implementations retain their backend and inherit its copy and move restrictions. */
struct LibraryQueries {
    virtual ~LibraryQueries() = default;
    virtual mc_result OpenPlaylist(mc_context*, const std::shared_ptr<const LibraryIdentity>&,
        int64_t, mc_track_search_completion_fn, void*, mc_request_id*) const = 0;
    virtual mc_result OpenSelection(mc_context*, const std::shared_ptr<const LibraryIdentity>&,
        std::string, std::string, int64_t, query::TrackSortType, mc_track_search_completion_fn, void*, mc_request_id*) const = 0;
    virtual mc_result ListPlaylists(mc_context*, mc_playlist_list_completion_fn, void*, mc_request_id*) const = 0;
    virtual mc_result SavePlaylist(mc_context*, int64_t, std::string, std::vector<int64_t>, bool append,
        mc_playlist_write_completion_fn, void*, mc_request_id*) const = 0;
    virtual mc_result RenamePlaylist(mc_context*, int64_t, std::string,
        mc_playlist_write_completion_fn, void*, mc_request_id*) const = 0;
    virtual mc_result DeletePlaylist(mc_context*, int64_t,
        mc_playlist_write_completion_fn, void*, mc_request_id*) const = 0;
};

struct LibraryIdentity {
    std::string locator;
    std::shared_ptr<const query::TrackReadBackend> backend;
    std::shared_ptr<const AsyncTrackReader> async;
    std::shared_ptr<const PlaybackOrigin> playbackOrigin;
    std::shared_ptr<const LibraryQueries> queries;
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
