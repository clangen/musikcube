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
//
////////////////////////////////////////////////////////////////////////////

#pragma once

/// @file OpenMptIndexerSource.h
/// @brief Indexer source that adds tracked-music modules to the library.
/// @details Scans configured directories for files with supported tracker
/// extensions and writes the resulting tracks to the library index with stable
/// "libopenmpt" external ids. Invalid files are remembered to avoid re-parsing
/// them on every scan.

#include <musikcore/sdk/IIndexerSource.h>
#include <functional>
#include <set>
#include <map>
#include <atomic>

/** @brief Indexes tracked-music module files into the library.
 *  @details Implements IIndexerSource. Each supported module becomes one
 *  indexed track (or several, when the module contains multiple sub-tracks).
 *  A running scan can be interrupted from another thread. */
class OpenMptIndexerSource: public musik::core::sdk::IIndexerSource {
    public:
        /** @brief Constructs an empty indexer source. */
        OpenMptIndexerSource();
        /** @brief Destroys the source. */
        ~OpenMptIndexerSource();

        /* IIndexerSource */
        /** @brief Destroys the source. */
        virtual void Release();
        /** @brief Prepares the source for a scan. */
        virtual void OnBeforeScan();
        /** @brief Cleans up after a scan completes. */
        virtual void OnAfterScan();
        /** @brief Returns the stable source id.
         *  @return The source id. */
        virtual int SourceId();

        /** @brief Scans for module files and writes their tracks to the index.
         *  @param indexer The indexer writer to add tracks to.
         *  @param indexerPaths Paths the indexer is configured to scan.
         *  @param indexerPathsCount Number of indexer paths.
         *  @return The scan result. */
        virtual musik::core::sdk::ScanResult Scan(
            musik::core::sdk::IIndexerWriter* indexer,
            const char** indexerPaths,
            unsigned indexerPathsCount);

        /** @brief Adds metadata for a single indexed track.
         *  @param indexer The indexer writer.
         *  @param tagStore The tag store receiving track metadata.
         *  @param externalId The external id of the track. */
        virtual void ScanTrack(
            musik::core::sdk::IIndexerWriter* indexer,
            musik::core::sdk::ITagStore* tagStore,
            const char* externalId);

        /** @brief Interrupts a running scan. */
        virtual void Interrupt();

        /** @brief A metadata pass is required for each track.
         *  @return Always returns true. */
        virtual bool NeedsTrackScan() { return true; }

        /** @brief Track ids are stable across scans.
         *  @return Always returns true. */
        virtual bool HasStableIds() { return true; }

    private:
        /** @brief Writes metadata for one module file into the index.
         *  @param fn The module file path.
         *  @param source The indexer source providing the metadata.
         *  @param indexer The indexer writer. */
        void UpdateMetadata(
            std::string fn,
            musik::core::sdk::IIndexerSource* source,
            musik::core::sdk::IIndexerWriter* indexer);

        /** @brief Files known to be unreadable or unparsable. */
        std::set<std::string> invalidFiles;
        /** @brief Paths scanned on the last pass. */
        std::set<std::string> paths;
        /** @brief Counters of files and tracks indexed. */
        size_t filesIndexed, tracksIndexed;
        /** @brief Set to interrupt an in-progress scan. */
        std::atomic<bool> interrupt { false };
};