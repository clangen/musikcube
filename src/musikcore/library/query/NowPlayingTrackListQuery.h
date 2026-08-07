//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

/** @file NowPlayingTrackListQuery.h
 *  @brief Query that returns the current playback queue as a track list.
 *  @details Snapshot of the tracks currently in the playback queue, used by
 *      views that mirror the "now playing" queue. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/audio/PlaybackService.h>

#include "TrackListQueryBase.h"

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Exposes the playback queue as a query result.
     *  @details Copies the tracks from the given PlaybackService's playlist and
     *      exposes them with headers and a query hash. */
    class NowPlayingTrackListQuery : public TrackListQueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            DELETE_CLASS_DEFAULTS(NowPlayingTrackListQuery)

            /** @brief Creates a now-playing track list query.
             *  @param library The library used to resolve track metadata.
             *  @param playback The playback service holding the queue. */
            NowPlayingTrackListQuery(
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback);

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

            /* TrackListQueryBase */
            /** @return The result track list (or nullptr). */
            Result GetResult() noexcept override;
            /** @return The column headers for the result. */
            Headers GetHeaders() noexcept override;
            /** @return A hash identifying this query's parameters. */
            size_t GetQueryHash() noexcept override;
            /** @return An empty durations map (durations are not provided). */
            Durations GetDurations() noexcept override {
                return std::make_shared<std::map<size_t, size_t>>();
            }

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            musik::core::ILibraryPtr library; /**< Library for track lookups. */
            musik::core::audio::PlaybackService& playback; /**< Playback service holding the queue. */
            Result result;   /**< Result track list. */
            Headers headers; /**< Result column headers. */
            size_t hash;     /**< Cached query hash. */
    };

} } } }
