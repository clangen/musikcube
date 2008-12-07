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

#include <core/config_filesystem.h>
#include <core/Track.h>
#include <core/Library/Base.h>
#include <boost/weak_ptr.hpp>

//////////////////////////////////////////////////////////////////////////////
namespace musik{ namespace core{
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///A GenericTrack is not related to any library. It must contain a URI
//////////////////////////////////////////
class GenericTrack : public Track {
    public:
        static TrackPtr Create(const utfchar *uri);
    protected:
        GenericTrack(void);
        GenericTrack(const utfchar *uri);
    public:
        virtual ~GenericTrack(void);

        virtual const utfchar* GetValue(const char* metakey);
        virtual void SetValue(const char* metakey,const utfchar* value);
        virtual void ClearValue(const char* metakey);
        virtual void SetThumbnail(const char *data,long size);
        virtual const utfchar* URI();
        virtual const utfchar* URL();

        virtual MetadataIteratorRange GetValues(const char* metakey);
        virtual MetadataIteratorRange GetAllValues();
        virtual TrackPtr Copy();

    private:
        // The variables
        utfstring uri;
        Track::MetadataMap metadata;
    
        typedef boost::weak_ptr<Track> SelfWeakPtr;
        SelfWeakPtr selfPtr;
};


//////////////////////////////////////////////////////////////////////////////
} } // musik::core
//////////////////////////////////////////////////////////////////////////////


