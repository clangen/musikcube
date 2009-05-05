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

#include "../pch.hpp"
#include <core/tracklist/LibraryList.h>
#include <core/LibraryTrack.h>
#include <core/Query/SortTracks.h>

//////////////////////////////////////////////////////////////////////////////
using namespace musik::core::tracklist;
//////////////////////////////////////////////////////////////////////////////

LibraryList::LibraryList(musik::core::LibraryPtr library)
 :library(library)
 ,currentPosition(-1)
 ,maxCacheSize(100)
{
    this->hintedRows    = 10;
    this->metadataQuery.OnTracksEvent.connect(this,&LibraryList::OnTracksMetaFromQuery);
}


//////////////////////////////////////////
///\brief
///Get track from a specified position
///
///\param position
///Position in tracklist to get
///
///\returns
///shared pointer to track (could be a null pointer)
//////////////////////////////////////////
musik::core::TrackPtr LibraryList::operator [](long position){
    // Valid position?
    if(position>=0 && position<this->Size()){
        // If in cache?
        PositionCacheMap::iterator trackPosition    = this->positionCache.find(position);
        if(trackPosition!=this->positionCache.end()){
            return trackPosition->second;
        }
        return musik::core::TrackPtr(new LibraryTrack(this->tracklist[position],this->library));
    }
    return musik::core::TrackPtr();
}

//////////////////////////////////////////
///\brief
///Get track with metadata from a specified position
///
///\param position
///Position in tracklist to get
///
///\returns
///shared pointer to track (could be a null pointer)
///
///This is similar to the operator[], but will also request the metadata.
///If the track does not currently have any metadata, it will be signaled
///using the TrackMetadataUpdated event when data arrives.
///
///\see
///TrackMetadataUpdated
//////////////////////////////////////////
musik::core::TrackPtr LibraryList::TrackWithMetadata(long position){

    // check the positionCache if the track is in the cache already
    PositionCacheMap::iterator trackPosition    = this->positionCache.find(position);
    if(trackPosition!=this->positionCache.end()){
        return trackPosition->second;
    }

    // Load and add to cache
    this->LoadTrack(position);

    return (*this)[position];
}

//////////////////////////////////////////
///\brief
///Get the current track
//////////////////////////////////////////
musik::core::TrackPtr LibraryList::CurrentTrack(){
    if(this->currentPosition==-1 && this->Size()>0){
        this->SetPosition(0);
    }

    return (*this)[this->currentPosition];
}

//////////////////////////////////////////
///\brief
///Get the next track and increase the position.
//////////////////////////////////////////
musik::core::TrackPtr LibraryList::NextTrack(){
    long newPosition    = this->currentPosition+1;
    musik::core::TrackPtr nextTrack = (*this)[newPosition];
    if(nextTrack){
        this->SetPosition(newPosition);
    }
    return nextTrack;
}

//////////////////////////////////////////
///\brief
///Get the previous track and decrease the position.
//////////////////////////////////////////
musik::core::TrackPtr LibraryList::PreviousTrack(){
    long newPosition    = this->currentPosition-1;
    musik::core::TrackPtr nextTrack = (*this)[newPosition];
    if(nextTrack){
        this->SetPosition(newPosition);
    }
    return nextTrack;
}


//////////////////////////////////////////
///\brief
///Set a new position in the tracklist.
///
///\param position
///Position to set.
///
///\returns
///True if position is a valid one and successfully set.
//////////////////////////////////////////
bool LibraryList::SetPosition(long position){
    if(position>=0 && position<this->tracklist.size()){
        this->PositionChanged(position,this->currentPosition);
        this->currentPosition   = position;
        return true;
    }

    return false;
}

//////////////////////////////////////////
///\brief
///Get the current position. -1 if undefined.
//////////////////////////////////////////
long LibraryList::CurrentPosition(){
    return this->currentPosition;
}

//////////////////////////////////////////
///\brief
///Get current size of the tracklist. -1 if unknown.
//////////////////////////////////////////
long LibraryList::Size(){
    return (long)this->tracklist.size();
}

//////////////////////////////////////////
///\brief
///Clear the tracklist
//////////////////////////////////////////
void LibraryList::Clear(){
    this->tracklist.clear();
    this->ClearMetadata();
    this->TracklistChanged(true);
    this->PositionChanged(-1,this->currentPosition);
    this->currentPosition   = -1;
}

//////////////////////////////////////////
///\brief
///Clear the internal used cache
//////////////////////////////////////////
void LibraryList::ClearMetadata(){
    this->positionCache.clear();
    this->trackCache.clear();
}

//////////////////////////////////////////
///\brief
///Set (copy) tracklist from another tracklist
///
///\param tracklist
///tracklist to copy from
///
///\returns
///True if successfully copied.
//////////////////////////////////////////
bool LibraryList::operator =(musik::core::tracklist::Base &tracklist){
    this->Clear();

    // optimize if dynamic_cast works
    LibraryList *fromLibraryList    = dynamic_cast<LibraryList*>(&tracklist);
    if(fromLibraryList){
        if(fromLibraryList->library==this->library){
            this->tracklist         = fromLibraryList->tracklist;
            this->TracklistChanged(true);
            this->SetPosition(fromLibraryList->CurrentPosition());
            return true;
        }
        this->TracklistChanged(true);
        return false;
    }


    // The default way
    this->tracklist.reserve(tracklist.Size());

    int libraryId   = this->library->Id();
    bool success(false);
    // Loop through the tracks and copy everything
    for(long i(0);i<tracklist.Size();++i){
        musik::core::TrackPtr currentTrack  = tracklist[i];
        if(currentTrack->Id()==libraryId){
            // Same library, append
            this->tracklist.push_back(currentTrack->Id());
            success=true;
        }
    }
    this->TracklistChanged(true);
    this->SetPosition(tracklist.CurrentPosition());

    return success;
}

//////////////////////////////////////////
///\brief
///Append tracks from another tracklist
///
///It will append all tracks that comes from
///the same library and ignore the rest.
//////////////////////////////////////////
bool LibraryList::operator +=(musik::core::tracklist::Base &tracklist){

    this->tracklist.reserve(tracklist.Size()+this->Size());

    int libraryId   = this->library->Id();
    bool success(false);
    // Loop through the tracks and copy everything
    for(long i(0);i<tracklist.Size();++i){
        musik::core::TrackPtr currentTrack  = tracklist[i];
        if(currentTrack->Id()==libraryId){
            // Same library, append
            this->tracklist.push_back(currentTrack->Id());
            success=true;
        }
    }

    if(success){
        this->TracklistChanged(false);
    }

    return success;
}

//////////////////////////////////////////
///\brief
///Append a track to this tracklist
///
///\param track
///Track to add
///
///\returns
///True if successfully appended
//////////////////////////////////////////
bool LibraryList::operator +=(musik::core::TrackPtr track){
    if(this->library->Id()==track->LibraryId()){
        this->tracklist.push_back(track->Id());

        this->TracklistChanged(false);

        return true;
    }
    return false;
}

//////////////////////////////////////////
///\brief
///Get related library. Null pointer if non.
//////////////////////////////////////////
musik::core::LibraryPtr LibraryList::Library(){
    return this->library;
}



//////////////////////////////////////////
///\brief
///Load a tracks metadata (if not already loaded) 
//////////////////////////////////////////
void LibraryList::LoadTrack(long position){
    if(this->QueryForTrack(position)){
        // If the track should load, then preload others as well

        int trackCount(1);

        // Preload hintedRows
        for(long i(position+1);i<position+this->hintedRows;++i){
            if(this->QueryForTrack(i)){
                ++trackCount;
            }
        }

        // Send the query to the library
        this->library->AddQuery(this->metadataQuery,musik::core::Query::Prioritize);

        // Finally, clear the query for futher metadata
        this->metadataQuery.Clear();
        
        ////////////////////////////////////////
        // Lets see if the cache is too big
        if(this->positionCache.size()>this->maxCacheSize){
            // Cache too big, what end should be removed
            PositionCacheMap::iterator front    = this->positionCache.begin();
            PositionCacheMap::iterator back     = this->positionCache.end();
            back--;

            if(position-front->first < back->first-position){
                // closer to beginning, lets erase in the end
                while(this->positionCache.size()>this->maxCacheSize){
                    back        = this->positionCache.end();
                    back--;
                    this->trackCache.erase(back->second);
                    this->positionCache.erase(back);
                }
            }else{
                // closer to end, lets erase in the beginning
                while(this->positionCache.size()>this->maxCacheSize){
                    front        = this->positionCache.begin();
                    this->trackCache.erase(front->second);
                    this->positionCache.erase(front);
                }
            }

        }

    }
}

//////////////////////////////////////////
///\brief
///Request metadata for track
//////////////////////////////////////////
bool LibraryList::QueryForTrack(long position){
    PositionCacheMap::iterator trackIt=this->positionCache.find(position);
    if(trackIt==this->positionCache.end()){
        // Not in cache, lets find the track
        musik::core::TrackPtr track = (*this)[position];
        if(track){
            // Track is also a valid track, lets add it to the cache
            this->positionCache[position]   = track;
            this->trackCache[track]         = position;

            // Finally, lets add it to the query
            this->metadataQuery.RequestTrack(track);
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////
///\brief
///Connect to receive results from any query based on the musik::core::Query::ListBase
//////////////////////////////////////////
bool LibraryList::ConnectToQuery(musik::core::Query::ListBase &query){
    query.OnTrackEvent().connect(this,&LibraryList::OnTracksFromQuery);
    query.OnTrackInfoEvent().connect(this,&LibraryList::OnTracksSummaryFromQuery);
    return true;
}


//////////////////////////////////////////
///\brief
///Receive new tracks from a query
///
///\param newTracks
///The new tracks
///
///\param clear
///Should the tracklist be cleared before tracks are added.
//////////////////////////////////////////
void LibraryList::OnTracksFromQuery(musik::core::TrackVector *newTracks,bool clear){
    if(clear){
        this->positionCache.clear();
        this->trackCache.clear();
        this->tracklist.clear();

        this->SetPosition(-1);   // undefined
    }

    // Copy to tracklist
    for(musik::core::TrackVector::iterator track=newTracks->begin();track!=newTracks->end();++track){
        this->tracklist.push_back( (*track)->Id() );
    }

    this->TracklistChanged(true);
}

//////////////////////////////////////////
///\brief
///Called by connected queries to receive summary from tracklist
//////////////////////////////////////////
void LibraryList::OnTracksSummaryFromQuery(UINT64 tracks,UINT64 duration,UINT64 filesize){
    this->SummaryInfoUpdated(tracks,duration,filesize);
}


//////////////////////////////////////////
///\brief
///Receives what tracks now have metadata, and pass them forward to the TrackMetadataUpdated signal
//////////////////////////////////////////
void LibraryList::OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks){
    std::vector<long> updateTrackPositions;

    for(musik::core::TrackVector::iterator track=metaTracks->begin();track!=metaTracks->end();++track){
        TrackCacheMap::iterator position    = this->trackCache.find(*track);
        if(position!=this->trackCache.end()){
            updateTrackPositions.push_back(position->second);
        }
    }

    this->TrackMetadataUpdated(updateTrackPositions);
}


//////////////////////////////////////////
///\brief
///Request another metakey
//////////////////////////////////////////
bool LibraryList::AddRequestedMetakey(std::string metakey){
    this->requestedMetakeys.insert(metakey);
    this->metadataQuery.RequestMetakeys(this->requestedMetakeys);

    return true;
}


//////////////////////////////////////////
///\brief
///Sort the tracks in the tracklist
///
///The method will not wait for the tracklist to be sorted.
///Instead the query will be send to the library for parsing.
//////////////////////////////////////////
bool LibraryList::SortTracks(std::string sortingMetakey){
    musik::core::Query::SortTracks sortQuery;
    sortQuery.AddTracks(this->tracklist);
    sortQuery.OnTrackEvent().connect(this,&LibraryList::OnTracksFromQuery);

    std::list<std::string> sortBy;
    sortBy.push_back(sortingMetakey);

    sortQuery.SortByMetaKeys(sortBy);
    this->library->AddQuery(sortQuery);
    return true;
}

