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

#include <core/Query/Base.h>
#include <core/MetadataValue.h>
#include <core/Track.h>

#include <sigslot/sigslot.h>
#include <vector>
#include <map>
#include <set>
#include <boost/shared_ptr.hpp>

//////////////////////////////////////////////////////////////
// Forward declarations
//////////////////////////////////////////////////////////////


namespace musik{ namespace core{

    namespace Query{
        class ListBase : public Query::Base {
            public:
                ListBase(void);
                virtual ~ListBase(void);

                typedef std::map<std::string,MetadataValueVector> MetadataResults;
                typedef sigslot::signal2<MetadataValueVector*,bool> MetadataSignal;
                typedef std::map<std::string,MetadataSignal> MetadataSignals;
                typedef sigslot::signal2<TrackVector*,bool> TrackSignal;
                typedef sigslot::signal3<UINT64,UINT64,UINT64> TrackInfoSignal;

            protected:
                friend class Library::Base;
                friend class Library::LocalDB;
                friend class server::Connection;

                bool RunCallbacks(Library::Base *library);

                bool ParseTracksSQL(std::string sql,Library::Base *library,db::Connection &db);

                bool RecieveQueryStandardNodes(musik::core::xml::ParserNode &node);
                bool SendQueryStandardNodes(musik::core::xml::WriterNode &queryNode);

                MetadataResults metadataResults;
                TrackVector trackResults;

                std::set<std::string> clearedMetadataResults;
                bool clearedTrackResults;

                MetadataSignals metadataEvent;
                TrackSignal trackEvent;
                TrackInfoSignal trackInfoEvent;

                UINT64 trackInfoTracks;
                UINT64 trackInfoDuration;
                UINT64 trackInfoSize;

                virtual bool SendResults(musik::core::xml::WriterNode &queryNode,Library::Base *library);
                virtual bool RecieveResults(musik::core::xml::ParserNode &queryNode,Library::Base *library);

            public:
                MetadataSignal& OnMetadataEvent(const char* metatag);
                MetadataSignal& OnMetadataEvent(const wchar_t* metatag);
                TrackSignal& OnTrackEvent();
                TrackInfoSignal& OnTrackInfoEvent();
            public:
                void DummySlot(MetadataValueVector*,bool);
                void DummySlotTracks(TrackVector*,bool);
                void DummySlotTrackInfo(UINT64,UINT64,UINT64);
        };
    }
} }


