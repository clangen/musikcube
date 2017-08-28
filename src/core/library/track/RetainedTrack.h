//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <core/sdk/IRetainedTrack.h>
#include <core/sdk/IRetainedTrackWriter.h>
#include "Track.h"
#include <atomic>

namespace musik { namespace core {

    class RetainedTrack : public musik::core::sdk::IRetainedTrack {
        public:
            RetainedTrack(TrackPtr track);
            virtual ~RetainedTrack();

            /* IRetainedTrack */
            virtual void Release();
            virtual void Retain();

            /* ITrack */
            virtual int64_t GetId();
            virtual int GetValue(const char* key, char* dst, int size);
            virtual long long GetInt64(const char* key, long long defaultValue = 0LL);
            virtual int GetInt32(const char* key, unsigned int defaultValue = 0);
            virtual double GetDouble(const char* key, double defaultValue = 0.0f);
            virtual int Uri(char* dst, int size);

        private:
            std::atomic<int> count;
            TrackPtr track;
    };

    class RetainedTrackWriter : public musik::core::sdk::IRetainedTrackWriter {
        public:
            RetainedTrackWriter(TrackPtr track);
            virtual ~RetainedTrackWriter();

            template <typename T> T As() {
                return dynamic_cast<T>(track.get());
            }

            /* IRetainedTrackWriter */
            virtual void Release();
            virtual void Retain();

            /* ITrackWriter */
            virtual void SetValue(const char* metakey, const char* value);
            virtual void ClearValue(const char* metakey);
            virtual void SetThumbnail(const char *data, long size);

        private:
            std::atomic<int> count;
            TrackPtr track;
    };

} }

