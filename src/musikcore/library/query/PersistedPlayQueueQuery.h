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

/** @file PersistedPlayQueueQuery.h
 *  @brief Query that saves or restores the playback queue to/from the library.
 *  @details Serializes the current playback queue into the database (so it can be
 *      restored after a restart) or restores it from the database. */

#include <musikcore/library/QueryBase.h>
#include <musikcore/audio/PlaybackService.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Persists or restores the playback queue.
     *  @details Create with Save() to write the queue into the library, or with
     *      Restore() to load it back into the PlaybackService. */
    class PersistedPlayQueueQuery : public musik::core::library::query::QueryBase {
        public:
            static const std::string kQueryName; /**< Query type name. */

            /** @brief Creates a query that saves the queue to the library.
             *  @param library The library to write to.
             *  @param playback The playback service whose queue is saved.
             *  @return A new query (caller owns it). */
            static PersistedPlayQueueQuery* Save(
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback)
            {
                return new PersistedPlayQueueQuery(library, playback, Type::Save);
            }

            /** @brief Creates a query that restores the queue from the library.
             *  @param library The library to read from.
             *  @param playback The playback service to populate.
             *  @return A new query (caller owns it). */
            static PersistedPlayQueueQuery* Restore(
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback)
            {
                return new PersistedPlayQueueQuery(library, playback, Type::Restore);
            }

            DELETE_CLASS_DEFAULTS(PersistedPlayQueueQuery)

            /* IQuery */
            /** @return The query type name. */
            std::string Name() override { return kQueryName; }

        protected:
            /* QueryBase */
            /** @brief Runs the query against the database.
             *  @param db The connection to run on.
             *  @return true on success. */
            bool OnRun(musik::core::db::Connection &db) override;

        private:
            /** @brief Whether this query saves or restores the queue. */
            enum class Type { Save, Restore };

            /** @brief Constructs the query (use Save()/Restore()).
             *  @param library The library to read or write.
             *  @param playback The playback service.
             *  @param type Save or Restore. */
            PersistedPlayQueueQuery(
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback,
                Type type) noexcept;

            musik::core::ILibraryPtr library; /**< Library to read or write. */
            musik::core::audio::PlaybackService& playback; /**< Playback service. */
            Type type; /**< Save or Restore. */
    };

} } } }
