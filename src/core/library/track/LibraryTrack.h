//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/config.h>
#include <core/library/track/Track.h>
#include <core/library/LocalLibrary.h>
#include <core/db/Connection.h>
#include <mutex>

namespace musik { namespace core {

    class LibraryTrack : public Track {
        public:
            LibraryTrack();
            LibraryTrack(int64_t id, int libraryId);
            LibraryTrack(int64_t id, musik::core::ILibraryPtr library);
            virtual ~LibraryTrack();

            virtual int LibraryId();

            virtual int64_t GetId();
            virtual void SetId(int64_t id) { this->id = id; }

            virtual std::string GetString(const char* metakey);
            virtual std::string Uri();

            /* ITagStore */
            virtual void SetValue(const char* metakey, const char* value);
            virtual void ClearValue(const char* metakey);
            virtual bool Contains(const char* metakey);
            virtual void SetThumbnail(const char *data, long size);
            virtual bool ContainsThumbnail();
            virtual void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain);

            /* ITrack */
            virtual long long GetInt64(const char* key, long long defaultValue = 0LL);
            virtual int GetInt32(const char* key, unsigned int defaultValue = 0);
            virtual double GetDouble(const char* key, double defaultValue = 0.0f);
            virtual int GetString(const char* key, char* dst, int size);
            virtual int Uri(char* dst, int size);

            virtual MetadataIteratorRange GetValues(const char* metakey);
            virtual MetadataIteratorRange GetAllValues();
            virtual TrackPtr Copy();

            static bool Load(Track *target, db::Connection &db);

        private:
            int64_t id;
            int libraryId;
            Track::MetadataMap metadata;
            std::mutex mutex;
    };

} }
