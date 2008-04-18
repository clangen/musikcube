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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <core/tracklist/IRandomAccess.h>
#include <core/Query/ListBase.h>
#include <core/Query/Tracks.h>
#include <core/Library/Base.h>

#include <sigslot/sigslot.h>
#include <boost/shared_ptr.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include <set>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{
    namespace tracklist {

        class Standard : public IRandomAccess, public sigslot::has_slots<> {

            public:
                
                typedef boost::shared_ptr<Standard> Ptr;

                Standard(void);
                ~Standard(void);

                musik::core::TrackPtr CurrentTrack();
                musik::core::TrackPtr NextTrack();
                musik::core::TrackPtr PreviousTrack();

                musik::core::TrackPtr operator [](int position);
                int Size();
                void SetCurrentPosition(int position);
                int CurrentPosition();

                void ConnectToQuery(musik::core::Query::ListBase &listQuery);
                void SetLibrary(musik::core::LibraryPtr setLibrary);
                musik::core::LibraryPtr Library();

                void HintNumberOfRows(int rows);

                void AddRequestedMetakey(const char* metakey);
                void RemoveRequestedMetakey(const char* metakey);

                void CopyTracks(musik::core::tracklist::IRandomAccess &tracklist);
                void AppendTracks(musik::core::tracklist::IRandomAccess &tracklist);

                typedef sigslot::signal1<bool> TracksEvent;
                TracksEvent OnTracks;
                typedef sigslot::signal1<std::vector<int>&> TrackMetaEvent;
                TrackMetaEvent OnTrackMeta;

            protected:

                musik::core::TrackPtr Track(int position);

                std::set<std::string> requestedMetaKeys;

                void LoadTrack(int position);

                int currentPosition;

                musik::core::LibraryPtr library;

                musik::core::TrackVector tracks;

                musik::core::Query::Tracks trackQuery;

                void OnTracksFromQuery(musik::core::TrackVector *newTracks,bool clear);
                void OnTracksMetaFromQuery(musik::core::TrackVector *metaTracks);

                int hintedRows;


                struct CacheTrack{
                    CacheTrack(musik::core::TrackPtr track,int position) :
                        track(track),
                        position(position) 
                    {
                    };

                    int position;
                    musik::core::TrackPtr track;
                };

                struct tagPosition{};
                struct tagTrack{};
                struct tagPrio{};

                typedef boost::multi_index::ordered_unique<boost::multi_index::tag<tagPosition>,BOOST_MULTI_INDEX_MEMBER(CacheTrack,int,position)> MultiIndexPosition;
                typedef boost::multi_index::ordered_non_unique<boost::multi_index::tag<tagTrack>,BOOST_MULTI_INDEX_MEMBER(CacheTrack,musik::core::TrackPtr,track)> MultiIndexTrack;
                typedef boost::multi_index::indexed_by<MultiIndexPosition,MultiIndexTrack> MultiIndexBy;

                typedef boost::multi_index::multi_index_container<CacheTrack,MultiIndexBy> TrackCache;
                
                /*boost::multi_index::sequenced<tagPrio>,*/ 

                TrackCache trackCache;

                typedef boost::multi_index::index<TrackCache,tagTrack>::type CacheIndexTrack;
                typedef boost::multi_index::index<TrackCache,tagPosition>::type CacheIndexPosition;

                bool InCache(int position);
                bool InCache(musik::core::TrackPtr track);
        };
    }
} } // musik::core
