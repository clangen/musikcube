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

/** @file IIndexerWriter.h @brief Defines the IIndexerWriter interface for persisting indexed library data. */
#pragma once

#include "ITagStore.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    class IIndexerSource;

    /** @brief Writes indexed library data back to the application's metadata
     *  store on behalf of an IIndexerSource during a scan. */
    class IIndexerWriter {
        public:
            /** @brief Creates a writer for a single track's metadata.
             *  @return A tag store to populate with the track's metadata. */
            virtual ITagStore* CreateWriter() = 0;

            /** @brief Saves a populated track to the library.
             *  @param source The source that produced the track.
             *  @param track The tag store containing the track's metadata.
             *  @param externalId An optional external id to associate with the track.
             *  @return True if the track was saved. */
            virtual bool Save(IIndexerSource* source, ITagStore* track, const char* externalId = "") = 0;

            /** @brief Commits progress updates to the indexer UI.
             *  @param source The source that is scanning.
             *  @param updatedTracks The number of tracks updated since the last commit. */
            virtual void CommitProgress(IIndexerSource* source, unsigned updatedTracks = 0) = 0;

            /** @brief Removes tracks matching the given URI from the library.
             *  @param source The source associated with the tracks.
             *  @param uri The URI of the tracks to remove.
             *  @return True if any tracks were removed. */
            virtual bool RemoveByUri(IIndexerSource* source, const char* uri) = 0;

            /** @brief Removes a track matching the given external id from the library.
             *  @param source The source associated with the track.
             *  @param id The external id of the track to remove.
             *  @return True if the track was removed. */
            virtual bool RemoveByExternalId(IIndexerSource* source, const char* id) = 0;

            /** @brief Removes all tracks associated with the given source.
             *  @param source The source whose tracks should be removed.
             *  @return The number of tracks removed. */
            virtual int RemoveAll(IIndexerSource* source) = 0;

            /** @brief Returns the last known modification time of a track.
             *  @param source The source associated with the track.
             *  @param externalId The external id of the track.
             *  @return The modification timestamp, in milliseconds since the epoch. */
            virtual int64_t GetLastModifiedTime(IIndexerSource* source, const char* externalId) = 0;
    };

} } }
