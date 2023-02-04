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

#include "constants.h"
#include "IIndexerWriter.h"
#include "ITagStore.h"
#include <string>

namespace musik { namespace core { namespace sdk {

    class IIndexerSource {
        public:
            virtual void Release() = 0;

            virtual void OnBeforeScan() = 0;

            virtual void OnAfterScan() = 0;

            virtual ScanResult Scan(
                IIndexerWriter* indexer,
                const char** indexerPaths,
                unsigned indexerPathsCount) = 0;

            virtual void ScanTrack(
                IIndexerWriter* indexer,
                ITagStore* store,
                const char* externalId) = 0;

            virtual bool NeedsTrackScan() = 0;

            virtual void Interrupt() = 0;

            virtual bool HasStableIds() = 0;

            virtual int SourceId() = 0;
    };

    namespace indexer {
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

        template <typename String=std::string>
        static inline String createExternalId(const String& prefix, const String& fn, int track) {
            return prefix + "://" + std::to_string(track) + "/" + fn;
        }

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
