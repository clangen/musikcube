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

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <vector>
#include <list>
#include <string>

#include <core/config.h>
#include <core/library/query/ListQueryBase.h>
#include <core/library/query/LibraryTrackListQuery.h>

namespace musik { namespace core { namespace query {

    class SortTracksQuery : public query::ListQueryBase{
        public:
            SortTracksQuery();
            ~SortTracksQuery();

            void AddTrack(DBID trackId);
            void AddTracks(std::vector<DBID> &tracks);
            void AddTracks(musik::core::tracklist::LibraryTrackListQuery &tracks);

            void ClearTracks();

            void SortByMetaKeys(std::list<std::string> metaKeys);

        protected:
            friend class library::LibraryBase;
            friend class library::LocalLibrary;

            typedef std::vector<DBID> IntVector;
            typedef std::list<std::string> StringList;

            IntVector tracksToSort;
            StringList sortMetaKeys;

            bool SortTracksQuery::ParseQuery(library::LibraryBase *library, db::Connection &db);
            Ptr copy() const;

            virtual std::string Name();
    };

} } }


