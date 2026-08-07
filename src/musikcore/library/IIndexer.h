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

/** @file IIndexer.h
 *  @brief Abstract interface for the music library metadata indexer.
 *  @details Defines the contract for indexing configured paths into a library's
 *      database, including path management, scheduled syncs, progress reporting
 *      and lifecycle state. */

#include <string>
#include <vector>
#include <sigslot/sigslot.h>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {
    /** @brief Indexes music files from configured paths into a library database.
     *  @details Implementations scan configured directories, read tag metadata via
     *      ITagReader plugins, and write tracks into the library. Signals report
     *      indexing start, progress and completion. */
    class IIndexer {
        public:
            /** @brief Emitted when an indexing pass begins. */
            sigslot::signal0<> Started;
            /** @brief Emitted when indexing completes.
             *  @details The argument is the number of tracks processed. */
            sigslot::signal1<int> Finished;
            /** @brief Emitted periodically during indexing.
             *  @details The argument is the number of tracks scanned so far. */
            sigslot::signal1<int> Progress;

            /** @brief Lifecycle state of the indexer. */
            enum State {
                StateIdle = 0,     /**< Not indexing. */
                StateIndexing = 1, /**< Currently indexing. */
                StateStopping = 2, /**< Indexing is stopping. */
                StateStopped = 3   /**< Indexing has stopped. */
            };

            /** @brief Scope of a requested synchronization. */
            enum class SyncType {
                All = 0,     /**< Sync all sources. */
                Local = 1,   /**< Sync local sources only. */
                Rebuild = 2, /**< Rebuild the entire index. */
                Sources = 3  /**< Sync sources whose paths have changed. */
            };

            virtual ~IIndexer() { }
            /** @brief Adds a directory to be indexed.
             *  @param path The directory path to add. */
            virtual void AddPath(const std::string& path) = 0;
            /** @brief Removes a directory from the indexed paths.
             *  @param path The directory path to remove. */
            virtual void RemovePath(const std::string& path) = 0;
            /** @brief Retrieves the configured index paths.
             *  @param paths Output vector receiving the paths. */
            virtual void GetPaths(std::vector<std::string>& paths) = 0;
            /** @brief Schedules a synchronization of the given scope.
             *  @param type The scope of the sync to run. */
            virtual void Schedule(SyncType type) = 0;
            /** @brief Stops indexing and shuts the indexer down. */
            virtual void Shutdown() = 0;
            /** @return The current indexer state. */
            virtual State GetState() = 0;
    };
} }
