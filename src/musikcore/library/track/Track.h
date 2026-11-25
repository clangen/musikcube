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

#include <common/DeleteDefaults.h>
#include <musikcore/library/ILibrary.h>
#include <common/LegacyConstants.h>
#include <musikcore/plugin/metadata.h>
#include <atomic>
#include <vector>
#include <map>

namespace musik { namespace core {

    class Track;
    typedef std::shared_ptr<Track> TrackPtr;

    class Track {
        public:
            typedef std::multimap<std::string, std::string> MetadataMap;
            typedef std::pair<MetadataMap::iterator, MetadataMap::iterator> MetadataIteratorRange;

            virtual musik::core::ILibraryPtr Library() noexcept { return {}; }
            virtual int LibraryId() noexcept { return 0; }

            virtual ~Track() = default;

            /* metadata changes made while indexing. */
            virtual void SetValue(const char* key, const char* value) = 0;
            virtual void ClearValue(const char* key) = 0;
            virtual void SetThumbnail(const char* data, long size) = 0;
            virtual bool Contains(const char* key) = 0;
            virtual bool ContainsThumbnail() = 0;
            virtual void SetReplayGain(const mc_replay_gain_values& replayGain) = 0;

            virtual int64_t GetId() = 0;
            virtual mc_replay_gain_values GetReplayGain() = 0;
            virtual musik::core::sdk::MetadataState GetMetadataState() = 0;

            /* implementation specific */
            virtual void SetId(int64_t id) = 0;
            virtual std::string GetString(const char* metakey) = 0;
            virtual std::string Uri() = 0;
            virtual MetadataIteratorRange GetValues(const char* metakey) = 0;
            virtual MetadataIteratorRange GetAllValues() = 0;
            virtual TrackPtr Copy() = 0;
            virtual void SetMetadataState(musik::core::sdk::MetadataState state) = 0;

    };

} }
