//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/sdk/ITrackWriter.h>
#include <core/library/ILibrary.h>
#include <core/sdk/ITrack.h>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <map>

namespace musik { namespace core {

    class Track;
    typedef std::shared_ptr<Track> TrackPtr;
    typedef std::vector<TrackPtr> TrackVector;

    class Track :
        public musik::core::sdk::ITrackWriter,
        public musik::core::sdk::ITrack
    {
        public:
            typedef std::multimap<std::string, std::string> MetadataMap;
            typedef std::pair<MetadataMap::iterator, MetadataMap::iterator> MetadataIteratorRange;

            virtual ~Track();

            virtual unsigned long long GetId();
            virtual void SetId(DBID id) = 0;

            virtual musik::core::ILibraryPtr Library();
            virtual int LibraryId();

            virtual std::string GetValue(const char* metakey) = 0;
            virtual std::string Uri() = 0;

            virtual int GetValue(const char* key, char* dst, int size) = 0;
            virtual int Uri(char* dst, int size) = 0;

            virtual MetadataIteratorRange GetValues(const char* metakey) = 0;
            virtual MetadataIteratorRange GetAllValues() = 0;
            virtual TrackPtr Copy() = 0;
    };

} }
