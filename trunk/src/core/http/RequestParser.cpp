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

#include "pch.hpp"

#include <core/http/RequestParser.h>

#include <iostream>
#include <stdlib.h>

using namespace musik::core::http;

RequestParser::RequestParser()
{
}


RequestParser::~RequestParser()
{
}

void RequestParser::Parse(const std::string &request){
    this->Clear();

    // Find GET
    int getStartIndex    = request.find("GET ");
    if(getStartIndex != std::string::npos){
        getStartIndex+=4;
        // Find next whitespace
        int getEndIndex    = request.find_first_of(" \r\n",getStartIndex);
        if(getEndIndex!=std::string::npos){
            // a request is found
            this->fullRequest.assign(request,getStartIndex,getEndIndex-getStartIndex);

            // Find querystring
            int questionMark( this->fullRequest.find("?") );

            if(questionMark!=std::string::npos){
                this->path.assign(this->fullRequest.substr(questionMark));
            }else{
                this->path.assign(this->fullRequest);
            }
            this->SplitPath();
        }
    }
}

void RequestParser::SplitPath(){
    int startSearch(0);

    while(startSearch!=std::string::npos){
        int firstSlash( this->path.find("/",startSearch) );

        if(firstSlash==std::string::npos){
            if(this->path.size()-startSearch!=0){
                std::string matchPath;
                matchPath.assign(this->path,startSearch,this->path.size()-startSearch);
                this->splitPath.push_back(matchPath);
            }
            startSearch = std::string::npos;
        }else{
            if(firstSlash-startSearch!=0){
                std::string matchPath;
                matchPath.assign(this->path,startSearch,firstSlash-startSearch);
                this->splitPath.push_back(matchPath);
            }
            startSearch    = firstSlash+1;
        }
    }
}

void RequestParser::Clear(){
    this->path.clear();
    this->fullRequest.clear();
    this->splitPath.clear();
}
