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

#include "pch.hpp"

#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/support/Common.h>

using namespace musik::core;
using namespace musik::core::sdk;

LibraryTrack::LibraryTrack()
: id(0)
, libraryId(0)
, gain(nullptr)
, state(MetadataState::NotLoaded) {
}

LibraryTrack::LibraryTrack(int64_t id, int libraryId)
: id(id)
, libraryId(libraryId)
, gain(nullptr)
, state(MetadataState::NotLoaded) {
}

LibraryTrack::LibraryTrack(int64_t id, ILibraryPtr library)
: id(id)
, libraryId(library->Id())
, gain(nullptr)
, state(MetadataState::NotLoaded) {
}

LibraryTrack::~LibraryTrack() {
    delete gain;
    gain = nullptr;
}

std::string LibraryTrack::GetString(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    MetadataMap::iterator metavalue = this->metadata.find(metakey);
    while (metavalue != this->metadata.end()) {
        if (metavalue->second.size() > 0) {
            return metavalue->second;
        }
        ++metavalue;
    }
    return "";
}

long long LibraryTrack::GetInt64(const char* key, long long defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stoll(GetString(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

int LibraryTrack::GetInt32(const char* key, unsigned int defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stol(GetString(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

double LibraryTrack::GetDouble(const char* key, double defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stod(GetString(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

void LibraryTrack::SetValue(const char* metakey, const char* value) {
    if (value) {
        std::string strValue(value);
        if (strValue.size()) {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->metadata.insert(std::pair<std::string, std::string>(metakey, strValue));
        }

    }
}

void LibraryTrack::ClearValue(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    this->metadata.erase(metakey);
}

bool LibraryTrack::Contains(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return this->metadata.find(metakey) != this->metadata.end();
}

void LibraryTrack::SetThumbnail(const char *data, long size) {
    /* do nothing. we implement a fat interface; this is just used by
    IndexerTrack. */
}

bool LibraryTrack::ContainsThumbnail() {
    std::unique_lock<std::mutex> lock(this->mutex);
    auto it = this->metadata.find("thumbnail_id");
    if (it != this->metadata.end()) {
        return it->second.size() > 0;
    }
    return false;
}

void LibraryTrack::SetReplayGain(const ReplayGain& replayGain) {
    if (this->gain) {
        delete this->gain;
        this->gain = nullptr;
    }
    this->gain = new ReplayGain();
    *this->gain = replayGain;
}

MetadataState LibraryTrack::GetMetadataState() {
    return this->state;
}

void LibraryTrack::SetMetadataState(musik::core::sdk::MetadataState state) {
    this->state = state;
}

ReplayGain LibraryTrack::GetReplayGain() {
    if (this->gain) {
        return *gain;
    }
    ReplayGain gain;
    gain.albumGain = 1.0;
    gain.albumPeak = 1.0;
    gain.trackGain = 1.0;
    gain.trackPeak = 1.0;
    return gain;
}

std::string LibraryTrack::Uri() {
    return this->GetString("filename");
}

int LibraryTrack::GetString(const char* key, char* dst, int size) {
    return (int) CopyString(this->GetString(key), dst, size);
}

int LibraryTrack::Uri(char* dst, int size) {
    return (int) CopyString(this->Uri(), dst, size);
}

Track::MetadataIteratorRange LibraryTrack::GetValues(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return this->metadata.equal_range(metakey);
}

Track::MetadataIteratorRange LibraryTrack::GetAllValues() {
    return Track::MetadataIteratorRange(
        this->metadata.begin(),
        this->metadata.end());

    return Track::MetadataIteratorRange();
}

int64_t LibraryTrack::GetId() {
    return this->id;
}

int LibraryTrack::LibraryId() {
    return this->libraryId;
}

TrackPtr LibraryTrack::Copy() {
    return TrackPtr(new LibraryTrack(this->id, this->libraryId));
}
