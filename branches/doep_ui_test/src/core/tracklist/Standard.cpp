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
#include <core/tracklist/Standard.h>

using namespace musik::core::tracklist;


Standard::Standard(void) : currentPosition(0),visibleStartPosition(0),visibleCount(10){
    this->tracksQuery.OnTracksEvent.connect(this,&Standard::OnTracksMeta);

    std::set<std::string> defaultFields;
    defaultFields.insert("track");
    defaultFields.insert("title");
    defaultFields.insert("visual_genre");
    defaultFields.insert("visual_artist");
    defaultFields.insert("album");
    defaultFields.insert("duration");
    defaultFields.insert("year");
    
    this->SetTrackMetaKeys(defaultFields);
}

Standard::~Standard(void){
}

void Standard::OnTracksMeta(musik::core::TrackVector *tracks){
    std::vector<int> visiblePositions;
    //Hmm.. tricky. I'm not sure if this is the best way. do not know where to cache the position
    // Lets loop through the visible tracks.
    for(int position=this->visibleStartPosition;position<(this->visibleStartPosition+this->visibleCount) && position<this->tracks.size();++position){
        bool found(false);
        for(musik::core::TrackVector::iterator track=tracks->begin();track!=tracks->end() && !found;++track){
            if(this->tracks[position]->id==(*track)->id){
                found   = true;
                visiblePositions.push_back(position);
            }
        }
    }

    if(!visiblePositions.empty()){
        this->OnVisibleTracksMetadataEvent(visiblePositions);
    }

}


musik::core::TrackPtr Standard::operator [](int position){
    boost::mutex::scoped_lock lock(this->mainMutex);

    if(0<=position && position<this->tracks.size()){
        return this->tracks[position];
    }

    return musik::core::TrackPtr();
}

int Standard::Size(){
    boost::mutex::scoped_lock lock(this->mainMutex);
    return this->tracks.size();
}

musik::core::TrackPtr Standard::CurrentTrack(){
    boost::mutex::scoped_lock lock(this->mainMutex);
    if(0<=this->currentPosition && this->currentPosition<this->tracks.size()){
        return this->tracks[this->currentPosition];
    }

    return musik::core::TrackPtr();
}

musik::core::TrackPtr Standard::NextTrack(){
    this->SetCurrentPosition(this->CurrentPosition()+1);
    return this->CurrentTrack();
}

musik::core::TrackPtr Standard::PreviousTrack(){
    this->SetCurrentPosition(this->CurrentPosition()-1);
    return this->CurrentTrack();
}

int Standard::CurrentPosition(){
    boost::mutex::scoped_lock lock(this->mainMutex);
    return this->currentPosition;
}

void Standard::SetCurrentPosition(int position){
    boost::mutex::scoped_lock lock(this->mainMutex);
    this->currentPosition    = position;
}


void Standard::ConnectToQuery(musik::core::Query::ListBase &query){
    query.OnTrackEvent().connect(this,&Standard::OnTracks);
}

void Standard::OnTracks(musik::core::TrackVector *tracks,bool clear){
    boost::mutex::scoped_lock lock(this->mainMutex);
    if(clear){
        this->ClearTrackCache();
        this->tracks.clear();
    }
    this->tracks.insert(this->tracks.end(),tracks->begin(),tracks->end());

    this->LoadVisible();
}

void Standard::SetTrackMetaKeys(std::set<std::string> metaKeys){
    this->tracksQuery.RequestMetakeys(metaKeys);
}

void Standard::SetVisibleTracks(int startPosition,int count){
    this->visibleStartPosition  = startPosition;
    this->visibleCount          = count;

    this->LoadVisible();
}


void Standard::SetTrackCache(int maxTracksCached){
    this->maxTracksCache    = maxTracksCached;
}

void Standard::ClearTrackCache(){
    for(std::list<musik::core::TrackPtr>::iterator track=this->tracksCache.begin(); track!=this->tracksCache.end();){
        if( track->use_count()>2 ){
            ++track;
        }else{
            (*track)->ClearMeta();
            track   = this->tracksCache.erase(track);
        }
    }
}

void Standard::LoadVisible(){
    bool tracksAdded(false);
    for(int position=this->visibleStartPosition;position<(this->visibleStartPosition+this->visibleCount) && position<this->tracks.size();++position){
        TrackPtr track(this->tracks[position]);
        if(!track->HasMeta()){
            tracksAdded=true;
            this->tracksQuery.RequestTrack(track);
            this->tracksCache.push_back(track);
        }
    }
    if(tracksAdded){
        this->library->AddQuery(this->tracksQuery);
    }
}

