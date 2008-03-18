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
#include <core/Library/Base.h>
#include <core/Track.h>
#include <core/Query/ListBase.h>
#include <core/Query/Tracks.h>

#include <vector>
#include <map>
#include <set>
#include <boost/function.hpp>
#include <sigslot/sigslot.h>

namespace musik{ namespace core{
    namespace PlayList{

        class Base : private sigslot::has_slots<>{
            public:
                Base(void);
                virtual ~Base(void);

                void Init(musik::core::Library::Base *library);


                void AddTrack(musik::core::TrackPtr track,int position=-1);
                void AddTracks(musik::core::TrackVector *tracks,int position=-1);
                void RemoveTracks(int startPosition,int endPosition);
                void Clear();

                void ConnectToQuery(musik::core::Query::ListBase *listQuery);

                int Size();

                void GetTrackMeta(int startPosition,int endPosition);
                void SetMetaFields(std::set<std::string> fields);

                musik::core::TrackPtr operator [](int position);
                musik::core::TrackPtr Track(int position);

            protected:

                musik::core::TrackVector tracks;
                musik::core::Query::Tracks trackQuery;
                musik::core::Library::Base *library;

                int maxCache;
                int currentPosition;



                // Internal functions:
//                void _updateSongCache(std::vector<SongPtr> *aSongs);

                /* Add songs to the playlist */
//                void addSongs(SongIdList *aAddSongIds,bool bClear=false);

                /* Copy operator, used when copying to "now playing" */
//                void operator=(const PlayList::Base &oFrom);


                /* Get the song that is located in a specific position in the list.
                If the song isn't in the cache, it will be queried for and an empty
                pointer will be returned unless you decide to wait for it.*/
//                SongPtr getSongFromPosition(unsigned int iPos,bool bWait=false);
//                SongPtr getSongFromId(DBINT iId,bool bWait=false);

                /* Query for songs in a specific range. It will only query for songs not
                in cache already */
//                void cacheSongs(unsigned int iStartRange,unsigned int iEndRange);

                /* Get the number of songs in playlist */
//                unsigned int getCount();

                /* Set what range is visible. This is used to determin when to execute
                the onVisibleSongUpdate callback */
//                void setVisibleRange(unsigned int iStartRange,unsigned int iEndRange);

                /* Set library to send queries for songs */
//                void setLibrary(Library::Base *oNewLibraryPtr);



                // Callback functions:
/*                CallbackUpdatePosition onVisibleSongUpdate;
                CallbackRequestVisibleRange onRequestVisibleRange;

            private:
                void init();

            protected:

                Query::Songs oQuerySongs;

                unsigned int iVisibleRangeStart;
                unsigned int iVisibleRangeEnd;

                unsigned int iCurrentPosition;

                SongIdList aSongIdList;
                SongCache aSongCache;

                Library::Base *oLibraryPtr;*/
        };
    }
} }
