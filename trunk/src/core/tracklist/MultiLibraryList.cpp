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

#ifdef WIN32
#include "pch.hpp"
#else
#include <core/pch.hpp>
#endif

#include <core/tracklist/MultiLibraryList.h>
#include <core/LibraryTrack.h>
#include <core/LibraryFactory.h>
#include <core/NonLibraryTrackHelper.h>

#include <set>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::core::tracklist;

//////////////////////////////////////////////////////////////////////////////

MultiLibraryList::MultiLibraryList()
 :currentPosition(-1)
 ,inited(false)
 ,queryState(MultiLibraryList::Default)
{
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
musik::core::TrackPtr MultiLibraryList::operator [](long position){
    boost::mutex::scoped_lock lock(this->mutex);
    // Valid position?
    if(position>=0 && position<this->tracklist.size()){
        return this->tracklist[position];
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
musik::core::TrackPtr MultiLibraryList::TrackWithMetadata(long position){
    {
        boost::mutex::scoped_lock lock(this->mutex);

        // check the positionCache if the track is in the cache already
        PositionCacheMap::iterator trackPosition    = this->positionCache.find(position);
        if(trackPosition!=this->positionCache.end()){
            return trackPosition->second;
        }
    }

    // Load and add to cache
    this->LoadTrack(position);

    return (*this)[position];
}

//////////////////////////////////////////
///\brief
///Get the current track
//////////////////////////////////////////
musik::core::TrackPtr MultiLibraryList::CurrentTrack(){
    return (*this)[this->currentPosition];
}

//////////////////////////////////////////
///\brief
///Get the next track and increase the position.
//////////////////////////////////////////
musik::core::TrackPtr MultiLibraryList::NextTrack(){
    long newPosition;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        newPosition = this->currentPosition+1;
    }
    musik::core::TrackPtr nextTrack = (*this)[newPosition];
    this->SetPosition(newPosition);
    return nextTrack;
}

//////////////////////////////////////////
///\brief
///Get the previous track and decrease the position.
//////////////////////////////////////////
musik::core::TrackPtr MultiLibraryList::PreviousTrack(){
    long newPosition;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        newPosition = this->currentPosition-1;
    }
    musik::core::TrackPtr nextTrack = (*this)[newPosition];
    this->SetPosition(newPosition);
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
bool MultiLibraryList::SetPosition(long position){
    if(position>=-1 && position<=this->Size()){
        this->PositionChanged(position,this->currentPosition);
        {
            boost::mutex::scoped_lock lock(this->mutex);
            this->currentPosition   = position;
        }
        return true;
    }

    return false;
}

//////////////////////////////////////////
///\brief
///Get the current position. -1 if undefined.
//////////////////////////////////////////
long MultiLibraryList::CurrentPosition(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->currentPosition;
}

//////////////////////////////////////////
///\brief
///Get current size of the tracklist. -1 if unknown.
//////////////////////////////////////////
long MultiLibraryList::Size(){
    boost::mutex::scoped_lock lock(this->mutex);
    return (long)this->tracklist.size();
}

//////////////////////////////////////////
///\brief
///Clear the tracklist
//////////////////////////////////////////
void MultiLibraryList::Clear(){
    long oldPosition;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->tracklist.clear();
        oldPosition = this->currentPosition;
        this->currentPosition   = -1;
    }
    this->ClearMetadata();
    this->TracklistChanged(true);
    this->PositionChanged(-1,oldPosition);
}

void MultiLibraryList::ClearMetadata(){
    boost::mutex::scoped_lock lock(this->mutex);
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
bool MultiLibraryList::operator =(musik::core::tracklist::Base &tracklist){
    if(!this->inited){
        musik::core::NonLibraryTrackHelper::Instance().TrackMetadataUpdated.connect(this,&MultiLibraryList::OnTracksMetaFromNonLibrary);
        this->inited    = true;
    }
    if(&tracklist != this){
        this->Clear();
    
        {
            boost::mutex::scoped_lock lock(this->mutex);

            this->tracklist.reserve(tracklist.Size());

            // Loop through the tracks and copy everything
            for(long i(0);i<tracklist.Size();++i){
                this->tracklist.push_back(tracklist[i]->Copy());
            }
        }

        this->TracklistChanged(true);
        this->SetPosition(tracklist.CurrentPosition());
    }

    return true;
}

//////////////////////////////////////////
///\brief
///Append tracks from another tracklist
///
///It will append all tracks that comes from
///the same library and ignore the rest.
//////////////////////////////////////////
bool MultiLibraryList::operator +=(musik::core::tracklist::Base &tracklist){
    if(!this->inited){
        musik::core::NonLibraryTrackHelper::Instance().TrackMetadataUpdated.connect(this,&MultiLibraryList::OnTracksMetaFromNonLibrary);
        this->inited    = true;
    }

    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->tracklist.reserve(tracklist.Size()+this->tracklist.size());

        // Loop through the tracks and copy everything
        for(long i(0);i<tracklist.Size();++i){
            this->tracklist.push_back(tracklist[i]->Copy());
        }
    }

    this->TracklistChanged(false);

    return true;
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
bool MultiLibraryList::operator +=(musik::core::TrackPtr track){
    if(!this->inited){
        musik::core::NonLibraryTrackHelper::Instance().TrackMetadataUpdated.connect(this,&MultiLibraryList::OnTracksMetaFromNonLibrary);
        this->inited    = true;
    }

    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->tracklist.push_back(track->Copy());
    }

    this->TracklistChanged(false);

    return true;
}

//////////////////////////////////////////
///\brief
///Load a tracks metadata (if not already loaded) 
//////////////////////////////////////////
void MultiLibraryList::LoadTrack(long position){
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
        for(MetadataQueryMap::iterator query=this->metadataQueries.begin();query!=this->metadataQueries.end();++query){
            musik::core::LibraryPtr library = musik::core::LibraryFactory::Instance().GetLibrary(query->first);
            if(library){
                // Add the callbacks
                query->second.OnTracksEvent.connect(this,&MultiLibraryList::OnTracksMetaFromQuery);

                // What keys are requested
                query->second.RequestMetakeys(this->requestedMetakeys);

                // Execute the query
                library->AddQuery(query->second,musik::core::Query::Prioritize);
            }
        }

        // Finally, clear the query for futher metadata
        this->metadataQueries.clear();
    }
}

//////////////////////////////////////////
///\brief
///Request metadata for track
//////////////////////////////////////////
bool MultiLibraryList::QueryForTrack(long position){

    PositionCacheMap::iterator trackIt=this->positionCache.find(position);
    if(trackIt==this->positionCache.end()){
        // Not in cache, lets find the track
        musik::core::TrackPtr track = (*this)[position];
        if(track){
            // Track is also a valid track, lets add it to the cache
            this->positionCache[position]   = track;
            this->trackCache[track]         = position;

            // Finally, lets add it to the query
            this->metadataQueries[track->LibraryId()].RequestTrack(track);
            return true;
        }
    }
    return false;
}

void MultiLibraryList::OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks){
    std::vector<long> updateTrackPositions;

    {
        boost::mutex::scoped_lock lock(this->mutex);
        for(musik::core::TrackVector::iterator track=metaTracks->begin();track!=metaTracks->end();++track){
            TrackCacheMap::iterator position    = this->trackCache.find(*track);
            if(position!=this->trackCache.end()){
                updateTrackPositions.push_back(position->second);
            }
        }
    }

    this->TrackMetadataUpdated(updateTrackPositions);
}

void MultiLibraryList::OnTracksMetaFromNonLibrary(musik::core::TrackPtr track){
    std::vector<long> updateTrackPositions;

    {
        boost::mutex::scoped_lock lock(this->mutex);
        TrackCacheMap::iterator position    = this->trackCache.find(track);
        if(position!=this->trackCache.end()){
            updateTrackPositions.push_back(position->second);
        }
    }

    if(!updateTrackPositions.empty()){
        this->TrackMetadataUpdated(updateTrackPositions);
    }
}

bool MultiLibraryList::SortTracks(std::string sortingMetakey){
    boost::mutex::scoped_lock lock(this->mutex);

    this->queryState    = MultiLibraryList::Sorting;

    // Trick method. We need to sort al genericTracks by ourselfs
    // and send all the other tracks for sorting to it's own libraries
    SortTracksQueryMap queries;

    // Lets clear the generic track sortvector
    this->genericTrackSortVector.clear();

    // Start by looping through all the tracks
    for(TracklistVector::iterator track=this->tracklist.begin();track!=this->tracklist.end();++track){
        int libraryId   = (*track)->LibraryId();
        if(libraryId){
            // A library track, add to query
            queries[libraryId].AddTrack((*track)->Id());
        }else{
            // A generic track
            musik::core::Query::SortTracksWithData::TrackWithSortdata sortData;
            sortData.track  = *track;
            const utfchar *metavalue    = (*track)->GetValue(sortingMetakey.c_str());
            if(metavalue){
                sortData.sortData   = metavalue;
                boost::algorithm::to_lower(sortData.sortData);
            }
            this->genericTrackSortVector.push_back( sortData );
        }
    }

    this->sortQueryCount    = 0;
    this->sortResultMap.clear();

    // Check if there are any genericTracks
    if(!this->genericTrackSortVector.empty()){
        this->sortQueryCount++;
    }

    // Lets count the queries
    for(SortTracksQueryMap::iterator query=queries.begin();query!=queries.end();++query){
        musik::core::LibraryPtr lib = musik::core::LibraryFactory::Instance().GetLibrary(query->first);
        if(lib){
            this->sortQueryCount++;
        }
    }

    // So, lets send the tracks to the libraries for sorting
    for(SortTracksQueryMap::iterator query=queries.begin();query!=queries.end();++query){
        // First, connect to callbacks for results
        query->second.TrackResults.connect(this,&MultiLibraryList::OnTracksFromSortQuery);
        query->second.QueryFinished.connect(this,&MultiLibraryList::OnSortQueryFinished);
        query->second.SortByMetaKey(sortingMetakey);
        // Then send the query
        musik::core::LibraryPtr lib = musik::core::LibraryFactory::Instance().GetLibrary(query->first);
        if(lib){
            lib->AddQuery(query->second);
        }
    }

    // Then sort the generic tracks by hand
    if(!this->genericTrackSortVector.empty()){
        this->genericTrackSortVector.sort();
        this->sortQueryCount--;
        this->SortTheLists();
    }

    return true;
}

void MultiLibraryList::OnTracksFromSortQuery(musik::core::Query::SortTracksWithData::TrackWithSortdataVector *newTracks,bool clear){
    typedef musik::core::Query::SortTracksWithData::TrackWithSortdataVector SortDataVector;

    if(newTracks){
        if(!newTracks->empty()){
            int libraryId( newTracks->front().track->LibraryId() );
            if(libraryId){
                for(SortDataVector::iterator track=newTracks->begin();track!=newTracks->end();++track){
                    this->sortResultMap[libraryId].push_back(*track);
                }
            }
        }
    }
}

void MultiLibraryList::OnSortQueryFinished(musik::core::Query::Base *query,musik::core::Library::Base *library,bool success){
    if(success){
        this->sortQueryCount--;
        this->SortTheLists();
    }
}

void MultiLibraryList::SortTheLists(){
    if(this->sortQueryCount==0){
        // All queries should be finished, lets do the real sorting
        typedef std::multiset<SortHelper> SortHelperSet;
        SortHelperSet sortSet;
        // Insert all of the tracklists
        for(SortTracksResults::iterator result=this->sortResultMap.begin();result!=this->sortResultMap.end();++result){
            if(!result->second.empty()){
                sortSet.insert(SortHelper(result->second));
            }
        }

        // We need to insert the generic list as well
        if(!this->genericTrackSortVector.empty()){
            sortSet.insert(SortHelper(this->genericTrackSortVector));
        }

        // Clear the tracklist
        this->Clear();

        // while there is still tracks left, continue to feed
        while(!sortSet.empty()){
            SortHelperSet::iterator front   = sortSet.begin();
            this->tracklist.push_back(front->sortData.front().track);

            // Remove the track from the tracklist
            front->sortData.pop_front();

            if(front->sortData.empty()){
                // The list is empty, remove it
                sortSet.erase(front);
            }else{
                // For indexing in sortSet, remove and the add again
                SortHelper newSortHelper(*front);
                sortSet.erase(front);
                sortSet.insert(newSortHelper);
            }
        }
        this->TracklistChanged(true);
        this->PositionChanged(-1,this->currentPosition);
        this->currentPosition   = -1;

        this->sortResultMap.clear();
        this->genericTrackSortVector.clear();

    }
}

bool MultiLibraryList::SortHelper::operator<(const MultiLibraryList::SortHelper &sortHelper) const{
    return this->sortData.front().sortData < sortHelper.sortData.front().sortData;
}

