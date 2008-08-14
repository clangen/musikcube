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
#include <core/tracklist/Standard.h>


using namespace musik::core::tracklist;

Standard::Standard(void) 
:currentPosition(0)
,hintedRows(10)
,infoDuration(0)
,infoFilesize(0)
{
    this->trackQuery.OnTracksEvent.connect(this,&Standard::OnTracksMetaFromQuery);
}


Standard::~Standard(void){
}

musik::core::TrackPtr Standard::CurrentTrack(){
    return (*this)[this->currentPosition];
}


musik::core::TrackPtr Standard::NextTrack(){
    this->SetCurrentPosition(this->currentPosition+1);
    return this->CurrentTrack();
}


musik::core::TrackPtr Standard::PreviousTrack(){
    this->SetCurrentPosition(this->currentPosition-1);
    return this->CurrentTrack();
}


musik::core::TrackPtr Standard::TrackWithMetadata(int position){
    this->LoadTrack(position);
    return (*this)[position];
}

musik::core::TrackPtr Standard::operator [](int position){

    if(position>=0 && position<this->tracks.size())
        return this->tracks[position];

    if(position==-1 && this->tracks.size()>0)
        return this->tracks.front();

    return musik::core::TrackPtr();
}


int Standard::Size(){
    return this->tracks.size();
}


void Standard::SetCurrentPosition(int position){
    int lastPosition(this->currentPosition);
    if(position<-1){
        this->currentPosition   = -1;
    }else{
        if(position >= (int)this->tracks.size()){
            this->currentPosition   = this->tracks.size();
        }else{
            this->currentPosition   = position;
        }
    }
    if(this->currentPosition!=lastPosition){
        this->PositionChanged(this->currentPosition,lastPosition);
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
    listQuery.OnTrackInfoEvent().connect(this,&Standard::OnTracksInfoFromQuery);
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
        this->TracksUpdated(true);
    }else{
        this->tracks.insert(this->tracks.end(),newTracks->begin(),newTracks->end());
        this->TracksUpdated(false);
    }
}

void Standard::LoadTrack(int position){
    
    int trackCount(0);

    for(int i(position);i<position+this->hintedRows;++i){
        if(this->QueryForTrack(i)){
            ++trackCount;
        }
    }

    if(trackCount && this->library){
        this->library->AddQuery(this->trackQuery,musik::core::Query::Prioritize);
        this->trackQuery.Clear();
    }

}

bool Standard::QueryForTrack(int position){
    
    TrackCache::left_map::iterator trackIterator = this->trackCache.left.find(position);
    if(trackIterator==this->trackCache.left.end()){
        // Not in cache, lets find the track
        musik::core::TrackPtr track = (*this)[position];
        if(track){
            // Track is also a valid track, lets add it to the cache
            this->trackCache.insert( TrackCache::value_type(position,track) );

            // finally, lets add it to the query
            this->trackQuery.RequestTrack(track);
            return true;
        }
    }
    return false;
}

void Standard::HintNumberOfRows(int rows){
    this->hintedRows    = rows;
}


void Standard::OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks){
    std::vector<int> updateTrackPositions;

    for(musik::core::TrackVector::iterator track=metaTracks->begin();track!=metaTracks->end();++track){
        TrackCache::right_map::iterator trackPosition = this->trackCache.right.find(*track);
        if(trackPosition!=this->trackCache.right.end()){
            updateTrackPositions.push_back(trackPosition->second);
        }
    }

    this->TrackMetaUpdated(updateTrackPositions);
}


void Standard::AddRequestedMetakey(const char* metakey){
    this->requestedMetaKeys.insert(metakey);
    this->trackQuery.RequestMetakeys(this->requestedMetaKeys);
}


void Standard::RemoveRequestedMetakey(const char* metakey){
    this->requestedMetaKeys.erase(metakey);
    this->trackQuery.RequestMetakeys(this->requestedMetaKeys);
}

bool Standard::CopyTracks(musik::core::tracklist::Ptr tracklist){
	if(tracklist.get()!=this){   // Do not copy to itself
        this->trackCache.clear();
        this->SetLibrary(tracklist->Library());
        this->tracks.clear();
        this->tracks.reserve(tracklist->Size());
        for(int i(0);i<tracklist->Size();++i){
            this->tracks.push_back( (*tracklist)[i]->Copy());
        }
        this->SetCurrentPosition(tracklist->CurrentPosition());

        this->TracksUpdated(true);
        this->infoDuration  = tracklist->Duration();
        this->infoFilesize  = tracklist->Filesize();
        this->TracklistInfoUpdated(this->tracks.size(),this->infoDuration,this->infoFilesize);
    }
    return true;
}

bool Standard::AppendTracks(musik::core::tracklist::Ptr tracklist){
	if(this->library==tracklist->Library()){	// Only append to same library
		if(!this->library){	// If library is not set, set it.
			this->SetLibrary(tracklist->Library());
		}

		this->tracks.reserve(this->tracks.size()+tracklist->Size());

		for(int i(0);i<tracklist->Size();++i){
			this->tracks.push_back( (*tracklist)[i]->Copy());
		}

		this->TracksUpdated(false);
	    return true;
	}
    return false;
}

void Standard::OnTracksInfoFromQuery(UINT64 tracks,UINT64 duration,UINT64 filesize){
    this->infoDuration  = duration;
    this->infoFilesize  = filesize;
    this->TracklistInfoUpdated(tracks,duration,filesize);
}

UINT64 Standard::Duration(){
    return this->infoDuration;
}
UINT64 Standard::Filesize(){
    return this->infoFilesize;
}

