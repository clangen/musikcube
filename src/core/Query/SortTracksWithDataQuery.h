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

#include <core/config.h>
#include <core/Query/QueryBase.h>
#include <core/Track.h>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <vector>
#include <list>
#include <string>


namespace musik { namespace core { namespace query {
    
    //////////////////////////////////////////
    ///\brief
    ///SortTracksWithDataQuery is a query used to receive sorted tracklists along with the data the tracks are sorted by
    ///
    ///\remarks
    ///First concider to use the SortTracks query instead.
    ///
    ///\see
    ///musik::core::query::SortTracks
    //////////////////////////////////////////
    class SortTracksWithDataQuery : public query::QueryBase{
        public:
            //////////////////////////////////////////
            ///\brief
            ///The struct used to return both the track and the sorted data
            //////////////////////////////////////////
            struct TrackWithSortdata {
                musik::core::TrackPtr track;
                std::string sortData;
                bool operator<(const TrackWithSortdata &trackWithSortData) const;
            };

            typedef std::list<TrackWithSortdata> TrackWithSortdataVector;
            typedef sigslot::signal2<TrackWithSortdataVector*, bool> TrackWithdataSignal;

            SortTracksWithDataQuery();
            ~SortTracksWithDataQuery();

            void AddTrack(DBID trackId);
            void ClearTracks();
            void SortByMetaKey(std::string metaKey);

            TrackWithdataSignal TrackResults;
            TrackWithSortdataVector trackResults;

        protected:
            friend class library::LibraryBase;
            friend class library::LocalLibrary;
            typedef std::vector<DBID> IntVector;

            IntVector tracksToSort;
            std::string sortByMetaKey;
            bool clearedTrackResults;
            Ptr copy() const;
            bool RunCallbacks(library::LibraryBase *library);
            bool SortTracksWithDataQuery::ParseQuery(library::LibraryBase *library, db::Connection &db);

            virtual std::string Name();
    };

} } }
