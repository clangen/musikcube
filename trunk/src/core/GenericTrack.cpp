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

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

TrackPtr GenericTrack::Create(const utfchar *uri){
    GenericTrack *newTrack  = new GenericTrack(uri);
    TrackPtr track( newTrack );
    if(newTrack){
        newTrack->selfPtr   = track;
    }

    // Add this to the NonLibraryTrackHelper to read the metadata
    NonLibraryTrackHelper::Instance().ReadTrack(track);

    return track;
}


GenericTrack::GenericTrack(void)
{
}

GenericTrack::GenericTrack(const utfchar *uri)
{
    if(uri){
        this->uri   = uri;
    }
}

GenericTrack::~GenericTrack(void){
}

const utfchar* GenericTrack::GetValue(const char* metakey){
    if(metakey){
        std::string metaKey(metakey);
        {
            boost::mutex::scoped_lock lock(NonLibraryTrackHelper::TrackMutex());
            MetadataMap::iterator metavalue = this->metadata.find(metaKey);
            if(metavalue!=this->metadata.end()){
                return metavalue->second.c_str();
            }
        }

        if(metaKey=="title"){
            // In case there is no title
            utfstring::size_type lastSlash = this->uri.find_last_of(UTF("/\\"));
            if(lastSlash!=utfstring::npos){
                static utfstring tempString;
                tempString  = this->uri.substr(lastSlash+1);
                return tempString.c_str();
            }else{
                return this->uri.c_str();
            }
        }

        // Lets try prepend "visual_"
        if(metaKey.substr(0,7)=="visual_"){
            metaKey = metaKey.substr(7);
            return this->GetValue(metaKey.c_str());
        }
    }
    return NULL;
}

void GenericTrack::SetValue(const char* metakey,const utfchar* value){
    if(metakey && value){
        boost::mutex::scoped_lock lock(NonLibraryTrackHelper::TrackMutex());
        this->metadata.insert(std::pair<std::string,utfstring>(metakey,value));
    }
}

void GenericTrack::ClearValue(const char* metakey){
    boost::mutex::scoped_lock lock(NonLibraryTrackHelper::TrackMutex());
    this->metadata.erase(metakey);
}



void GenericTrack::SetThumbnail(const char *data,long size){
}

const utfchar* GenericTrack::URI(){
    return this->uri.c_str();
}

const utfchar* GenericTrack::URL(){
    return this->uri.c_str();
}

Track::MetadataIteratorRange GenericTrack::GetValues(const char* metakey){
    boost::mutex::scoped_lock lock(NonLibraryTrackHelper::TrackMutex());
    return this->metadata.equal_range(metakey);
}

Track::MetadataIteratorRange GenericTrack::GetAllValues(){
    return Track::MetadataIteratorRange(this->metadata.begin(),this->metadata.end());
}




TrackPtr GenericTrack::Copy(){
    // Do not copy generic tracks, try to return a selfPtr instead
    TrackPtr trackCopy;
    if(trackCopy = this->selfPtr.lock()){
        return trackCopy;
    }
    return GenericTrack::Create(this->uri.c_str());
}

