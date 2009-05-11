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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <core/tracklist/Base.h>
#include <core/Query/ListBase.h>
#include <core/Query/TrackMetadata.h>
#include <core/Library/Base.h>
#include <core/Query/SortTracksWithData.h>

#include <boost/thread/mutex.hpp>

#include <set>
#include <map>
#include <sigslot/sigslot.h>



//////////////////////////////////////////////////////////////////////////////
namespace musik{ namespace core{ namespace tracklist {
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///The MultiLibraryList can contain tracks from multiple libraries and GenericTracks
//////////////////////////////////////////
class MultiLibraryList : public Base, public sigslot::has_slots<> {
    public:
        MultiLibraryList();

        virtual musik::core::TrackPtr operator [](long position);
        virtual musik::core::TrackPtr TrackWithMetadata(long position);
        virtual musik::core::TrackPtr CurrentTrack();
        virtual musik::core::TrackPtr NextTrack();
        virtual musik::core::TrackPtr PreviousTrack();

        virtual bool SetPosition(long position);
        virtual long CurrentPosition();
        virtual long Size();
        virtual void Clear();

        virtual bool operator =(musik::core::tracklist::Base &tracklist);
        virtual bool operator +=(musik::core::tracklist::Base &tracklist);
        virtual bool operator +=(musik::core::TrackPtr track);

        virtual void ClearMetadata();
        virtual bool SortTracks(std::string sortingMetakey);

    private:
        void LoadTrack(long position);
        bool QueryForTrack(long position);

        void OnTracksSummaryFromQuery(UINT64 tracks,UINT64 duration,UINT64 filesize);
        void OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks);
        void OnTracksMetaFromNonLibrary(musik::core::TrackPtr track);

        void OnTracksFromSortQuery(musik::core::Query::SortTracksWithData::TrackWithSortdataVector *newTracks,bool clear);
        void OnSortQueryFinished(musik::core::Query::Base *query,musik::core::Library::Base *library,bool success);
        void SortTheLists();

    protected:
        //////////////////////////////////////////
		///\brief
		///Internal representation of the tracklist.
		//////////////////////////////////////////
        typedef std::vector<musik::core::TrackPtr> TracklistVector;
        TracklistVector tracklist;

    private:
		//////////////////////////////////////////
        // Cache
        typedef long PositionType;
        typedef std::map<PositionType,musik::core::TrackPtr> PositionCacheMap;
        PositionCacheMap positionCache;

        typedef std::map<musik::core::TrackPtr,PositionType> TrackCacheMap;
        TrackCacheMap trackCache;

		//////////////////////////////////////////

        long currentPosition;
        bool inited;

        // map with queries, to check for metadata
        typedef std::map<int,musik::core::Query::TrackMetadata> MetadataQueryMap;
        MetadataQueryMap metadataQueries;

		//////////////////////////////////////////
        // Sorting of tracks
        typedef enum {
            Default=0,
            Sorting=1
        } QueryState;

        int queryState;

        typedef std::map<int,musik::core::Query::SortTracksWithData> SortTracksQueryMap;
        typedef std::map<int,musik::core::Query::SortTracksWithData::TrackWithSortdataVector> SortTracksResults;
        SortTracksResults sortResultMap;
        int sortQueryCount;
        musik::core::Query::SortTracksWithData::TrackWithSortdataVector genericTrackSortVector;

        // Helper class for sorting
        struct SortHelper{
            SortHelper(musik::core::Query::SortTracksWithData::TrackWithSortdataVector &sortData):sortData(sortData){}   
            musik::core::Query::SortTracksWithData::TrackWithSortdataVector &sortData;

            bool operator<(const SortHelper &sortHelper) const;

        };
        
        //////////////////////////////////////
        ///\brief
        ///mutex protexting the internal tracklist
        //////////////////////////////////////
        boost::mutex mutex;
};

//////////////////////////////////////////////////////////////////////////////
} } } // musik::core::tracklist
//////////////////////////////////////////////////////////////////////////////
