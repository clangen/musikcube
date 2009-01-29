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

#include <core/filestreams/IFileStream.h>
#include <boost/shared_ptr.hpp>
#include "HTTPRequest.h"

using namespace musik::core::filestreams;
//////////////////////////////////////////////////////////////////////////////

class HTTPStream : public IFileStream
{
    public:
        HTTPStream();
        ~HTTPStream();
    public:
        virtual bool Open(const utfchar *filename,unsigned int options=0);
        virtual bool Close();
        virtual void Destroy();
        virtual PositionType Read(void* buffer,PositionType readBytes);
        virtual bool SetPosition(PositionType position);
        virtual PositionType Position();
        virtual bool Eof();
        virtual long Filesize();
        virtual const utfchar* Type();
    private:
        typedef boost::shared_ptr<HTTPRequest> HTTPRequestPtr;
        HTTPRequestPtr httpRequest;
        PositionType currentPosition;

        long cachedFilesize;
        utfstring cachedFilename;
        bool isEof;

};

//////////////////////////////////////////////////////////////////////////////


