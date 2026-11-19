#pragma once

#include "TrackReadQueries.h"

#include <memory>

namespace musik { namespace core { namespace library { namespace query {

    /* Noncopyable and nonmovable. Handles share the backend by reference. */
    class TrackReadBackend {
    public:
        TrackReadBackend() = default;
        virtual ~TrackReadBackend() = default;
        TrackReadBackend(const TrackReadBackend&) = delete;
        TrackReadBackend& operator=(const TrackReadBackend&) = delete;

        virtual bool Run(const TrackSearchQuery& query, TrackSearchRows& rows) const = 0;
        virtual bool Run(const TrackPageQuery& query, ProjectedTrackPage& page) const = 0;
    };

    std::shared_ptr<const TrackReadBackend> OpenTrackReadBackend(std::string databasePath);

} } } }
