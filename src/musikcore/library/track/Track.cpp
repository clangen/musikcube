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

#include "pch.hpp"

#include <musikcore/library/track/Track.h>
#include <musikcore/library/LocalLibrary.h>

using namespace musik::core;
using namespace musik::core::sdk;

/* * * * SdkWrapper * * * */

class SdkWrapper : public Track {
    public:
        DELETE_CLASS_DEFAULTS(SdkWrapper)

        SdkWrapper(TrackPtr track) noexcept {
            this->track = track;
            this->count = 1;
        }

        virtual ~SdkWrapper() {
        }

        void Retain() noexcept override {
            ++this->count;
        }

        void Release() noexcept override {
            const int c = this->count.fetch_sub(1);
            if (c == 1) { /* fetched before sub */
                this->count = 0;
                this->track.reset();
                delete this;
            }
        }

        int64_t GetId() override {
            return track->GetId();
        }

        int GetString(const char* key, char* dst, int size) override {
            return track->GetString(key, dst, size);
        }

        long long GetInt64(const char* key, long long defaultValue) override {
            return track->GetInt64(key, defaultValue);
        }

        int GetInt32(const char* key, unsigned int defaultValue) override {
            return track->GetInt32(key, defaultValue);
        }

        double GetDouble(const char* key, double defaultValue) override {
            return track->GetDouble(key, defaultValue);
        }

        int Uri(char* dst, int size) override {
            return track->Uri(dst, size);
        }

        IResource::Class GetClass() override {
            return track->GetClass();
        }

        const char* GetType() override {
            return track->GetType();
        }

        size_t GetValue(char* dst, size_t size) override {
            return track->GetValue(dst, size);
        }

        ReplayGain GetReplayGain() override {
            return track->GetReplayGain();
        }

        MetadataState GetMetadataState() override {
            return track->GetMetadataState();
        }

        /* pure virtual methods defined by Track, but not defined in ITrack. therefore,
        these methods cannot be called by the SDK, and should throw. */
        #define NO_IMPL throw std::runtime_error("not implemented");
        void SetValue(const char* key, const char* value) override { NO_IMPL }
        void ClearValue(const char* key) override { NO_IMPL }
        void SetThumbnail(const char *data, long size) override { NO_IMPL }
        bool Contains(const char* key) override { NO_IMPL }
        bool ContainsThumbnail() override { NO_IMPL }
        void SetReplayGain(const ReplayGain& replayGain) override { NO_IMPL }
        void SetId(int64_t id) override { NO_IMPL }
        std::string GetString(const char* metakey) override { NO_IMPL }
        std::string Uri() override { NO_IMPL }
        MetadataIteratorRange GetValues(const char* metakey) override { NO_IMPL }
        MetadataIteratorRange GetAllValues() override { NO_IMPL }
        TrackPtr Copy() override { NO_IMPL }
        void SetMetadataState(MetadataState state) override { NO_IMPL }
        #undef NO_IMPL

    private:
        std::atomic<int> count;
        std::shared_ptr<Track> track;
};

/* * * * Track * * * */

int64_t Track::GetId() {
    return 0;
}

ILibraryPtr Track::Library() noexcept {
    static ILibraryPtr nullLibrary;
    return nullLibrary;
}

int Track::LibraryId() noexcept{
    return 0;
}

void Track::Retain() noexcept {
    /* nothing. SdkWrapper implements as necessary */
}

void Track::Release() noexcept {
    /* same as Retain() */
}

ITrack* Track::GetSdkValue() {
    return new SdkWrapper(shared_from_this());
}

IResource::Class Track::GetClass() {
    return IResource::Class::Map;
}

const char* Track::GetType() {
    return "track";
}

size_t Track::GetValue(char* dst, size_t count) {
    return 0;
}

/* * * * TagStore * * * */

template<typename T>
struct NoDeleter {
    void operator()(T* t) noexcept {
    }
};

TagStore::TagStore(TrackPtr track) noexcept {
    this->count = 1;
    this->track = track;
}

TagStore::TagStore(Track& track) {
    this->count = 1;
    this->track = TrackPtr(&track, NoDeleter<Track>());
}

void TagStore::SetValue(const char* key, const char* value) {
    this->track->SetValue(key, value);
}

void TagStore::ClearValue(const char* key) {
    this->track->ClearValue(key);
}

bool TagStore::Contains(const char* key) {
    return this->track->Contains(key);
}

bool TagStore::ContainsThumbnail() {
    return this->track->ContainsThumbnail();
}

void TagStore::SetThumbnail(const char *data, long size) {
    this->track->SetThumbnail(data, size);
}

void TagStore::Release() noexcept {
    const int c = this->count.fetch_sub(1);
    if (c == 1) { /* fetched before sub */
        this->count = 0;
        this->track.reset();
        delete this;
    }
}

void TagStore::SetReplayGain(const ReplayGain& replayGain) {
    this->track->SetReplayGain(replayGain);
}

void TagStore::Retain() noexcept {
    ++this->count;
}
