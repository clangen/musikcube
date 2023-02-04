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

#include <musikcore/config.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/db/Connection.h>
#include <mutex>
#include <atomic>

namespace musik { namespace core {

    class LibraryTrack: public Track {
        public:
            LibraryTrack() noexcept;
            LibraryTrack(int64_t id, int libraryId);
            LibraryTrack(int64_t id, musik::core::ILibraryPtr library);
            virtual ~LibraryTrack();

            int LibraryId() noexcept override;

            int64_t GetId() noexcept override;
            void SetId(int64_t id) noexcept override { this->id = id; }

            std::string GetString(const char* metakey) override;
            std::string Uri() override;

            /* ITagStore */
            void SetValue(const char* metakey, const char* value) override;
            void ClearValue(const char* metakey) override;
            bool Contains(const char* metakey) override;
            void SetThumbnail(const char* data, long size) override;
            bool ContainsThumbnail() override;
            void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) override;

            /* ITrack */
            long long GetInt64(const char* key, long long defaultValue = 0LL) override;
            int GetInt32(const char* key, unsigned int defaultValue = 0) override;
            double GetDouble(const char* key, double defaultValue = 0.0f) override;
            int GetString(const char* key, char* dst, int size) override;
            int Uri(char* dst, int size) override;
            musik::core::sdk::ReplayGain GetReplayGain() noexcept override;
            musik::core::sdk::MetadataState GetMetadataState() noexcept override;
            void SetMetadataState(musik::core::sdk::MetadataState state) noexcept override;

            MetadataIteratorRange GetValues(const char* metakey) override;
            MetadataIteratorRange GetAllValues() noexcept override;
            TrackPtr Copy() override;

        private:
            int64_t id;
            int libraryId;
            Track::MetadataMap metadata;
            std::mutex mutex;
            std::atomic<musik::core::sdk::MetadataState> state;
            musik::core::sdk::ReplayGain* gain;
    };

} }
