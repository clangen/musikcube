//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

#include <core/GenericTrack.h>
#include <core/NonLibraryTrackHelper.h>

using namespace musik::core;

TrackPtr GenericTrack::Create(const char *uri) {
    GenericTrack *newTrack  = new GenericTrack(uri);
    TrackPtr track(newTrack);
    newTrack->selfPtr = track; /* used to make another shared_ptr */
    NonLibraryTrackHelper::Instance().ReadTrack(track);
    return track;
}

GenericTrack::GenericTrack() {
}

GenericTrack::GenericTrack(const char *uri) {
    if (uri) {
        this->uri = uri;
    }
}

GenericTrack::~GenericTrack() {
}

const char* GenericTrack::GetValue(const char* metakey) {
    if (metakey) {
        std::string metaKey(metakey);

        {
            boost::mutex::scoped_lock lock(this->metadataMutex);
            MetadataMap::iterator metavalue = this->metadata.find(metaKey);
            if(metavalue != this->metadata.end()) {
                return metavalue->second.c_str();
            }
        }

        if (metaKey == "title") {
            if (this->title.size()) {
                return this->title.c_str();
            }

            std::string::size_type lastSlash = this->uri.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                this->title = this->uri.substr(lastSlash+1);
                return title.c_str();
            }
            else {
                return this->uri.c_str();
            }
        }

        /* TODO: figure out -- i have no idea what this means... */
        if(metaKey.substr(0,7) == "visual_") {
            metaKey = metaKey.substr(7);
            return this->GetValue(metaKey.c_str());
        }
    }

    return "";
}

void GenericTrack::SetValue(const char* metakey, const char* value){
    if (metakey && value) {
        boost::mutex::scoped_lock lock(this->metadataMutex);
        this->metadata.insert(std::pair<std::string, std::string>(metakey,value));
    }
}

void GenericTrack::ClearValue(const char* metakey) {
    boost::mutex::scoped_lock lock(this->metadataMutex);
    this->metadata.erase(metakey);
}

void GenericTrack::SetThumbnail(const char *data,long size) {
}

const char* GenericTrack::URI() {
    return this->uri.c_str();
}

const char* GenericTrack::URL() {
    return this->uri.c_str();
}

Track::MetadataIteratorRange GenericTrack::GetValues(const char* metakey) {
    boost::mutex::scoped_lock lock(this->metadataMutex);
    return this->metadata.equal_range(metakey);
}

Track::MetadataIteratorRange GenericTrack::GetAllValues() {
    return Track::MetadataIteratorRange(this->metadata.begin(),this->metadata.end());
}

TrackPtr GenericTrack::Copy() {
    TrackPtr trackCopy;
    if (trackCopy = this->selfPtr.lock()) {
        return trackCopy;
    }

    return GenericTrack::Create(this->uri.c_str());
}

