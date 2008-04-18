//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, mC2 team
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
#include "core/tracklist/Standard.h"


using namespace musik::core::tracklist;

Standard::Standard(void) : currentPosition(0),hintedRows(10){
    this->trackQuery.OnTracksEvent.connect(this,&Standard::OnTracksMetaFromQuery);

}


Standard::~Standard(void){
}

musik::core::TrackPtr Standard::CurrentTrack(){
    return this->Track(this->currentPosition);
}


musik::core::TrackPtr Standard::NextTrack(){
    this->SetCurrentPosition(this->currentPosition+1);
    return this->CurrentTrack();
}


musik::core::TrackPtr Standard::PreviousTrack(){
    this->SetCurrentPosition(this->currentPosition-1);
    return this->CurrentTrack();
}


musik::core::TrackPtr Standard::operator [](int position){

    if(position>=0 && position<this->tracks.size())
        this->LoadTrack(position);
        return this->tracks[position];

    if(position==-1)
        this->LoadTrack(0);
        return this->tracks.front();

    return musik::core::TrackPtr();
}

musik::core::TrackPtr Standard::Track(int position){

    if(position>=0 && position<this->tracks.size())
        return this->tracks[position];

    if(position==-1)
        return this->tracks.front();

    return musik::core::TrackPtr();
}


int Standard::Size(){
    return this->tracks.size();
}


void Standard::SetCurrentPosition(int position){
    if(position<-1){
        this->currentPosition   = -1;
    }else{
        if(position >= (int)this->tracks.size()){
            this->currentPosition   = this->tracks.size()-1;
        }else{
            this->currentPosition   = position;
        }
    }
}


int Standard::CurrentPosition(){
    if(this->currentPosition<0)
        return -1;

    if(this->currentPosition >= (int)this->tracks.size())
        return this->tracks.size()-1;

    return this->currentPosition;
}

void Standard::ConnectToQuery(musik::core::Query::ListBase &listQuery){
    listQuery.OnTrackEvent().connect(this,&Standard::OnTracksFromQuery);
}


void Standard::SetLibrary(musik::core::LibraryPtr setLibrary){
    this->library   = setLibrary;
}

musik::core::LibraryPtr Standard::Library(){
    return this->library;
}



void Standard::OnTracksFromQuery(musik::core::TrackVector *newTracks,bool clear){
    if(clear){
        this->trackCache.clear();
        this->SetCurrentPosition(-1);   // undefined
        this->tracks   = *newTracks;
        this->OnTracks(true);
    }else{
        this->tracks.insert(this->tracks.end(),newTracks->begin(),newTracks->end());
        this->OnTracks(false);
    }
}

void Standard::LoadTrack(int position){

    if(!this->InCache(position)){
        // Not in cache
        // Lets load the hinted number of tracks forward
        int trackCount(0);
        
        for(int i(position);i<position+this->hintedRows;++i){
            if(!this->InCache(i)){
                // Not in cache, load the track and add to Cache
                musik::core::TrackPtr track = this->Track(i);
                if(track){
                    this->trackCache.insert(CacheTrack(track,i));
                    ++trackCount;
                    this->trackQuery.RequestTrack(track);
                }
            }
        }

        if(trackCount && this->library){
            this->library->AddQuery(this->trackQuery,musik::core::Query::Prioritize);
            this->trackQuery.Clear();
        }

    }

}

void Standard::HintNumberOfRows(int rows){
    this->hintedRows    = rows;
}

bool Standard::InCache(int position){
    CacheIndexPosition& indexPosition = boost::multi_index::get<tagPosition>(this->trackCache);

    if( indexPosition.find(position) == indexPosition.end() )
        return false;

    return true;
}

bool Standard::InCache(musik::core::TrackPtr track){
    CacheIndexTrack& indexTrack = boost::multi_index::get<tagTrack>(this->trackCache);

    if( indexTrack.find(track) == indexTrack.end() )
        return false;

    return true;
}

void Standard::OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks){
    std::vector<int> updateTrackPositions;
    CacheIndexTrack& indexTrack = boost::multi_index::get<tagTrack>(this->trackCache);

    for(musik::core::TrackVector::iterator track=metaTracks->begin();track!=metaTracks->end();++track){
        CacheIndexTrack::iterator cacheTrackIterator = indexTrack.find(*track);
        if(cacheTrackIterator!=indexTrack.end()){
            updateTrackPositions.push_back(cacheTrackIterator->position);
        }
    }

    this->OnTrackMeta(updateTrackPositions);
}


void Standard::AddRequestedMetakey(const char* metakey){
    this->requestedMetaKeys.insert(metakey);
    this->trackQuery.RequestMetakeys(this->requestedMetaKeys);
}


void Standard::RemoveRequestedMetakey(const char* metakey){
    this->requestedMetaKeys.erase(metakey);
    this->trackQuery.RequestMetakeys(this->requestedMetaKeys);
}

void Standard::CopyTracks(musik::core::tracklist::IRandomAccess &tracklist){
    if(this!=&tracklist){   // Do not copy to itself
        this->SetLibrary(tracklist.Library());
        this->tracks.clear();
        this->tracks.reserve(tracklist.Size());
        for(int i(0);i<tracklist.Size();++i){
            this->tracks.push_back(tracklist.Track(i)->Copy());
        }
        this->SetCurrentPosition(tracklist.CurrentPosition());

        this->OnTracks(true);
    }
}

void Standard::AppendTracks(musik::core::tracklist::IRandomAccess &tracklist){
    if(!this->library){
        this->SetLibrary(tracklist.Library());
    }

    this->tracks.reserve(this->tracks.size()+tracklist.Size());

    for(int i(0);i<tracklist.Size();++i){
        this->tracks.push_back(tracklist.Track(i)->Copy());
    }

    this->OnTracks(false);
}


