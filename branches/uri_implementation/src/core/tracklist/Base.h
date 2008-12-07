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

#include <core/config.h>
#include <core/Track.h>
#include <core/Library/Base.h>
#include <core/Query/ListBase.h>

//////////////////////////////////////////////////////////////////////////////

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <set>

//////////////////////////////////////////////////////////////////////////////
namespace musik{ namespace core{ namespace tracklist {
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///The virtual base that sets up the required methods for a tracklist
//////////////////////////////////////////
class Base : boost::noncopyable{
    public:
        virtual ~Base();

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
		virtual musik::core::TrackPtr operator [](long position) = 0;

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
		virtual musik::core::TrackPtr TrackWithMetadata(long position)=0;

        //////////////////////////////////////////
		///\brief
		///Get the current track
		//////////////////////////////////////////
		virtual musik::core::TrackPtr CurrentTrack()=0;

        //////////////////////////////////////////
		///\brief
		///Get the next track and increase the position.
		//////////////////////////////////////////
		virtual musik::core::TrackPtr NextTrack()=0;

        //////////////////////////////////////////
		///\brief
		///Get the previous track and decrease the position.
		//////////////////////////////////////////
		virtual musik::core::TrackPtr PreviousTrack()=0;

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
		virtual bool SetPosition(long position)=0;

        //////////////////////////////////////////
		///\brief
		///Get the current position. -1 if undefined.
		//////////////////////////////////////////
		virtual long CurrentPosition()=0;

        //////////////////////////////////////////
		///\brief
		///Get current size of the tracklist. -1 if unknown.
		//////////////////////////////////////////
		virtual long Size() = 0;

        //////////////////////////////////////////
		///\brief
		///Clear the tracklist
		//////////////////////////////////////////
		virtual void Clear() = 0;

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
		virtual bool operator =(musik::core::tracklist::Base &tracklist) = 0;

        //////////////////////////////////////////
		///\brief
		///Append a tracklist to this tracklist
		///
		///\param tracklist
		///tracklist to append to this one
		///
		///\returns
		///True if successfully appended
		//////////////////////////////////////////
		virtual bool operator +=(musik::core::tracklist::Base &tracklist) = 0;

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
		virtual bool operator +=(musik::core::TrackPtr track) = 0;


        //////////////////////////////////////////
		///\brief
		///Get related library. Null pointer if non.
		//////////////////////////////////////////
		virtual musik::core::LibraryPtr Library();

        //////////////////////////////////////////
		///\brief
		///Sort tracks by a metakey
		//////////////////////////////////////////
        virtual bool SortTracks(std::string sortingMetakey);

        //////////////////////////////////////////
		///\brief
		///Connect the tracklist to recieve tracks from a ListBase query.
		//////////////////////////////////////////
        virtual bool ConnectToQuery(musik::core::Query::ListBase &query);

        //////////////////////////////////////////
		///\brief
		///Add a metakey to the list of metakeys to
        ///get when requesting TrackWithMetadata
		//////////////////////////////////////////
        virtual bool AddRequestedMetakey(std::string metakey);


        //////////////////////////////////////////
		///\brief
		///Make a hint on how may rows that are visible
		//////////////////////////////////////////
        virtual void HintVisibleRows(long rows);

        //////////////////////////////////////////
		///\brief
		///Clears the metadata cache
		//////////////////////////////////////////
        virtual void ClearMetadata();


        /////////////////////////////////////////////////////////////////////////////
        // EVENTS
        /////////////////////////////////////////////////////////////////////////////


        //////////////////////////////////////////
		typedef sigslot::signal1<std::vector<long>> TrackMetadataEvent;
        //////////////////////////////////////////
		///\brief
		///Event, called when metadata has been recieved.
		///
        ///The event reciever will get a std::vector<long>& with
        ///a vector of positions of tracks that has been updated.
		//////////////////////////////////////////
		TrackMetadataEvent TrackMetadataUpdated;


        //////////////////////////////////////////
        typedef sigslot::signal3<UINT64,UINT64,UINT64> SummaryInfoEvent;
        //////////////////////////////////////////
		///\brief
		///Event, called when summary has been updated
		///
        ///The event reciever will get 3 UINT64 parameters
		//////////////////////////////////////////
        SummaryInfoEvent SummaryInfoUpdated;


        //////////////////////////////////////////
        typedef sigslot::signal1<bool> TracklistChangedEvent;
        //////////////////////////////////////////
		///\brief
		///Event, called when tracks inside the tracklist has been added/changed
		///
        ///The event reciever will get a bool set to true if the list has been cleared first
		//////////////////////////////////////////
        TracklistChangedEvent TracklistChanged;


        //////////////////////////////////////////
        typedef sigslot::signal2<long,long> PositionChangedEvent;
        //////////////////////////////////////////
		///\brief
		///Event, called when position in tracklist has been updated
		///
        ///The event reciever will get the nex position, and the old position
		//////////////////////////////////////////
        PositionChangedEvent PositionChanged;

    protected:
        std::set<std::string> requestedMetakeys;
        long hintedRows;

};

typedef boost::shared_ptr<Base> Ptr;

//////////////////////////////////////////////////////////////////////////////
} } } // musik::core
//////////////////////////////////////////////////////////////////////////////

