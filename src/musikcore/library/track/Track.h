//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <musikcore/support/DeleteDefaults.h>
#include <musikcore/sdk/ITagStore.h>
#include <musikcore/library/ILibrary.h>
#include <musikcore/sdk/ITrack.h>
#include <atomic>
#include <vector>
#include <map>

namespace musik { namespace core {

    class Track;
    typedef std::shared_ptr<Track> TrackPtr;

    class Track :
        public musik::core::sdk::ITrack,
        public std::enable_shared_from_this<Track>
    {
        public:
            typedef std::multimap<std::string, std::string> MetadataMap;
            typedef std::pair<MetadataMap::iterator, MetadataMap::iterator> MetadataIteratorRange;

            EXPORT virtual musik::core::ILibraryPtr Library() noexcept;
            EXPORT virtual int LibraryId() noexcept;

            /* ITrack is a ready only interface; we use the ITagStore interface
            for writing. we replicate the interface here, and have TagStore pass
            through to us */
            EXPORT virtual void SetValue(const char* key, const char* value) = 0;
            EXPORT virtual void ClearValue(const char* key) = 0;
            EXPORT virtual void SetThumbnail(const char* data, long size) = 0;
            EXPORT virtual bool Contains(const char* key) = 0;
            EXPORT virtual bool ContainsThumbnail() = 0;
            EXPORT virtual void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) = 0;

            /* IResource */
            EXPORT int64_t GetId() override;
            EXPORT Class GetClass() override;
            EXPORT const char* GetType() override;

            /* IValue */
            EXPORT size_t GetValue(char* dst, size_t size) override;

            /* IMap */
            EXPORT void Release() noexcept override;
            EXPORT int GetString(const char* key, char* dst, int size) override = 0;
            EXPORT long long GetInt64(const char* key, long long defaultValue = 0LL) override = 0;
            EXPORT int GetInt32(const char* key, unsigned int defaultValue = 0) override = 0;
            EXPORT double GetDouble(const char* key, double defaultValue = 0.0f) override = 0;

            /* ITrack */
            EXPORT void Retain() noexcept override;
            EXPORT int Uri(char* dst, int size) override = 0;

            /* implementation specific */
            EXPORT virtual void SetId(int64_t id) = 0;
            EXPORT virtual std::string GetString(const char* metakey) = 0;
            EXPORT virtual std::string Uri() = 0;
            EXPORT virtual MetadataIteratorRange GetValues(const char* metakey) = 0;
            EXPORT virtual MetadataIteratorRange GetAllValues() = 0;
            EXPORT virtual TrackPtr Copy() = 0;
            EXPORT virtual void SetMetadataState(musik::core::sdk::MetadataState state) = 0;

            /* for SDK interop */
            EXPORT ITrack* GetSdkValue();
    };

    class TagStore : public musik::core::sdk::ITagStore {
        public:
            DELETE_CLASS_DEFAULTS(TagStore)

            EXPORT TagStore(TrackPtr track) noexcept;
            EXPORT TagStore(Track& track);

            EXPORT virtual ~TagStore() noexcept { }

            template <typename T> T As() {
                return dynamic_cast<T>(track.get());
            }

            EXPORT void Retain() noexcept override;
            EXPORT void Release() noexcept override;
            EXPORT void SetValue(const char* key, const char* value) override;
            EXPORT void ClearValue(const char* key) override;
            EXPORT bool Contains(const char* key) override;
            EXPORT bool ContainsThumbnail() override;
            EXPORT void SetThumbnail(const char* data, long size) override;
            EXPORT void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) override;

        private:
            TrackPtr track;
            std::atomic<int> count;
    };

} }
