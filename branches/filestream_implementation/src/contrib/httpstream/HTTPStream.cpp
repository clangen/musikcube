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
#include "stdafx.h"
#include "HTTPStream.h"

#include <utf8/utf8.h>
#include <boost/lexical_cast.hpp>

HTTPStream::HTTPStream()
 : currentPosition(0)
{
}

HTTPStream::~HTTPStream()
{
}

bool HTTPStream::Open(const utfchar *filename,unsigned int options){
    this->currentPosition=0;
    if(!this->httpRequest){
        this->httpRequest.reset(new HTTPRequest());
    }
    std::wstring inputFilename(filename);
    std::string outputFilename;
    utf8::utf16to8(inputFilename.begin(),inputFilename.end(),std::back_inserter(outputFilename));

    return this->httpRequest->Request(outputFilename.c_str());
}

bool HTTPStream::Close(){
    this->httpRequest.reset();
    return false;
}

void HTTPStream::Destroy(){
    delete this;
}

PositionType HTTPStream::Read(void* buffer,PositionType readBytes){
    if(this->httpRequest){
        PositionType read   = this->httpRequest->GetContent(buffer,readBytes);
        this->currentPosition   += read;
        return read;
    }
    return 0;
}

bool HTTPStream::SetPosition(PositionType position){
    return false;
}

PositionType HTTPStream::Position(){
    return this->currentPosition;
}

bool HTTPStream::Eof(){
    return true;
}

long HTTPStream::Filesize(){
    try{
        std::string fileSizeString  = this->httpRequest->attributes["Content-Length"];
        return boost::lexical_cast<long>(fileSizeString);
    }
    catch(...){
        return 0;
    }
    return 0;
}



