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
// Forwar declare
namespace musik{ namespace core{
    class Track;
    namespace Library{
        class Base;
    }
} }
//////////////////////////////////////////////////////////////////////////////

#include <core/config.h>
#include <map>
#include <boost/utility.hpp>


//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{

//////////////////////////////////////////////////////////////////////////////

class TrackMeta : boost::noncopyable{
    public:
        TrackMeta(Library::Base *library);
        ~TrackMeta(void);


        typedef std::string Key;
        typedef utfstring Value;
        typedef std::multimap<Key,Value> TagMap;
        typedef TagMap::iterator TagMapIterator;
        typedef TagMap::const_iterator TagMapConstIterator;
        typedef std::pair<TagMapConstIterator,TagMapConstIterator> TagMapIteratorPair;
        typedef std::pair<Key,Value> TagMapPair;

    private:
        friend class Track;

        const utfstring& GetValue(const Key &key) const;
        TrackMeta::TagMapIteratorPair GetValues(const char* metakey) const;
        void SetValue(const Key &key,const Value &value);
        const utfchar* GetValue(const char* metakey) const;

        Library::Base *library;
        TrackMeta::TagMap tags;

        char *thumbnailData;
        unsigned int thumbnailSize;
        void SetThumbnail(const char *data,unsigned int size);

    private:
        const Value& _GetValue(const Key &key) const;

};

//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////
