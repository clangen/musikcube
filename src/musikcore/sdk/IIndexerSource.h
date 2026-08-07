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

/** @file IIndexerSource.h @brief Defines the IIndexerSource interface and helpers for external library scanning. */
#pragma once

#include "constants.h"
#include "IIndexerWriter.h"
#include "ITagStore.h"
#include <string>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A source of library metadata, such as a plugin-provided music
     *  database, that can be scanned and indexed by the application. */
    class IIndexerSource {
        public:
            /** @brief Releases the source; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Called before a scan pass begins. */
            virtual void OnBeforeScan() = 0;

            /** @brief Called after a scan pass completes. */
            virtual void OnAfterScan() = 0;

            /** @brief Performs a scan of the source's library data.
             *  @param indexer The writer used to persist scan results.
             *  @param indexerPaths The library paths that are being indexed.
             *  @param indexerPathsCount The number of library paths.
             *  @return The result of the scan pass. */
            virtual ScanResult Scan(
                IIndexerWriter* indexer,
                const char** indexerPaths,
                unsigned indexerPathsCount) = 0;

            /** @brief Scans a single track, reading its metadata.
             *  @param indexer The writer used to persist scan results.
             *  @param store The tag store to populate with the track's metadata.
             *  @param externalId The external id of the track to scan. */
            virtual void ScanTrack(
                IIndexerWriter* indexer,
                ITagStore* store,
                const char* externalId) = 0;

            /** @brief Returns whether the source requires per-track scanning.
             *  @return True if individual tracks must be scanned. */
            virtual bool NeedsTrackScan() = 0;

            /** @brief Interrupts any in-progress scan. */
            virtual void Interrupt() = 0;

            /** @brief Returns whether the source provides stable, persistent ids for its tracks.
             *  @return True if track ids remain stable across rescans. */
            virtual bool HasStableIds() = 0;

            /** @brief Returns the numeric id that identifies this source.
             *  @return The source id. */
            virtual int SourceId() = 0;
    };

    /** @brief Helpers for constructing and parsing external track ids. */
    namespace indexer {
        /** @brief Parses an external id into its file and track number components.
         *  @tparam String The string type.
         *  @param prefix The id prefix, e.g. "spotify".
         *  @param externalId The external id to parse.
         *  @param fn On return, receives the file path portion of the id.
         *  @param track On return, receives the track number portion of the id.
         *  @return True if the id was successfully parsed. */
        template <typename String=std::string>
        static bool parseExternalId(const String& prefix, const String& externalId, String& fn, int& track) {
            if (externalId.find(String(prefix + "://")) == 0) {
                String trimmed = externalId.substr(prefix.size() + 3);
                auto slash = trimmed.find("/");
                if (slash != String::npos) {
                    try {
                        track = std::stoi(trimmed.substr(0, slash));
                        fn = trimmed.substr(slash + 1);
                        return true;
                    }
                    catch (...) {
                        return false;
                    }
                }
            }
            return false;
        }

        /** @brief Creates an external id from a prefix, file path, and track number.
         *  @tparam String The string type.
         *  @param prefix The id prefix, e.g. "spotify".
         *  @param fn The file path portion of the id.
         *  @param track The track number portion of the id.
         *  @return The formatted external id. */
        template <typename String=std::string>
        static inline String createExternalId(const String& prefix, const String& fn, int track) {
            return prefix + "://" + std::to_string(track) + "/" + fn;
        }

        /** @brief Returns whether the file referenced by an external id still exists.
         *  @tparam String The string type.
         *  @param externalId The external id to check.
         *  @return True if the referenced file exists. */
        template <typename String=std::string>
        static inline bool externalIdExists(const String& externalId) {
            String fn;
            int trackNum;
            if (!parseExternalId(externalId, fn, trackNum)) {
                return false;
            }
            return fileExists(fn);
        }
    }

} } }
