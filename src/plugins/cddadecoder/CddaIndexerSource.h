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

///
/// @file CddaIndexerSource.h
/// @brief Indexer source that exposes audio-CD tracks to the library indexer.
/// @details During a scan the mounted audio discs are enumerated and each audio
/// track is added to the index with a stable "cdda://" external id. Windows-only.
///

#include <musikcore/sdk/IIndexerSource.h>
#include "CddaDataModel.h"

#include <functional>
#include <set>

/** @brief Indexes audio-CD tracks into the musikcube library.
 *  @details Implements IIndexerSource by walking the current set of mounted
 *  audio discs. Because the disc contents change, it listens for
 *  insertion/removal events and refreshes its track list accordingly.
 */
class CddaIndexerSource :
    public musik::core::sdk::IIndexerSource,
    public CddaDataModel::EventListener
{
    public:
        CddaIndexerSource();
        ~CddaIndexerSource();

        /* IIndexerSource */
        /** @brief Destroys the source. */
        void Release() override;
        /** @brief Prepares the source for a scan. */
        void OnBeforeScan() override;
        /** @brief Cleans up after a scan completes. */
        void OnAfterScan() override;
        /** @brief Returns the stable source id.
         *  @return The source id. */
        int SourceId() override;

        /** @brief Scans for audio discs and writes their tracks to the index.
         *  @param indexer The indexer writer to add tracks to.
         *  @param indexerPaths Paths the indexer is configured to scan.
         *  @param indexerPathsCount Number of indexer paths.
         *  @return The scan result. */
        musik::core::sdk::ScanResult Scan(
            musik::core::sdk::IIndexerWriter* indexer,
            const char** indexerPaths,
            unsigned indexerPathsCount) override;

        /** @brief Adds metadata for a single indexed track.
         *  @param indexer The indexer writer.
         *  @param tagStore The tag store receiving track metadata.
         *  @param externalId The cdda:// external id of the track. */
        void ScanTrack(
            musik::core::sdk::IIndexerWriter* indexer,
            musik::core::sdk::ITagStore* tagStore,
            const char* externalId) override;

        /** @brief Interrupts a running scan. */
        void Interrupt() override;
        /** @brief Track ids are stable across scans.
         *  @return Always returns true. */
        bool HasStableIds() noexcept override { return true; }
        /** @brief A track metadata pass is required.
         *  @return Always returns true. */
        bool NeedsTrackScan() noexcept override { return true; }

        /* CddaDataModel::EventListener */
        /** @brief Refreshes the track list when a disc is inserted/removed. */
        void OnAudioDiscInsertedOrRemoved() override;

    private:
        /** @brief Re-enumerates the mounted audio discs. */
        void RefreshModel();

        /** @brief Reference to the CDDA data model singleton. */
        CddaDataModel& model;
        /** @brief Set of known disc ids to detect changes. */
        std::set<std::string> discIds;
        /** @brief Currently mounted audio discs. */
        std::vector<CddaDataModel::AudioDiscPtr> discs;
};