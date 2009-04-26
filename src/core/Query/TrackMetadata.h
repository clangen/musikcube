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

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <core/Query/Base.h>
#include <core/config.h>
#include <core/Track.h>

#include <map>
#include <set>
//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace Query{

//////////////////////////////////////////////////////////////////////////////
// Forward declaration
class Base;
//////////////////////////////////////////////////////////////////////////////

class TrackMetadata : public Query::Base {
    public:
        TrackMetadata(void);
        ~TrackMetadata(void);

        bool RunCallbacks(Library::Base *library);

        void Clear();
        void RequestTrack(TrackPtr track);
        void RequestMetakeys(const std::set<std::string> &fields);
        void RequestAllMetakeys();

        typedef sigslot::signal1<TrackVector*> TrackMetadataEvent;
        TrackMetadataEvent OnTracksEvent;

    private:
        typedef std::set<std::string> StringSet;
        StringSet requestedFields;
        std::vector<std::string> fieldOrder;
        StringSet metaFields;
        StringSet categoryFields;
        std::string sSQL;
        std::string sSQLTables;
        std::string sSQLWhere;
        bool requestAllFields;

        TrackVector aRequestTracks;
        TrackVector aResultTracks;

        void CreateSQL();
        void GetFixedTrackMetakeys(std::string fieldName,std::set<std::string> &fields);
    protected:
        friend class Library::Base;
        friend class Library::LocalDB;
        Ptr copy() const;
        void PreAddQuery(Library::Base *library);

        virtual bool ParseQuery(Library::Base *library,db::Connection &db);

        virtual std::string Name();
        virtual bool ReceiveQuery(musik::core::xml::ParserNode &queryNode);
        virtual bool SendQuery(musik::core::xml::WriterNode &queryNode);
        virtual bool SendResults(musik::core::xml::WriterNode &queryNode,Library::Base *library);
        virtual bool ReceiveResults(musik::core::xml::ParserNode &queryNode,Library::Base *library);

};

//////////////////////////////////////////////////////////////////////////////
} } }   // musik::core::Query
//////////////////////////////////////////////////////////////////////////////

