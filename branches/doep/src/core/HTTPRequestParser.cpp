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

#include <core/HTTPRequestParser.h>

#include <iostream>
#include <stdlib.h>

using namespace musik::core;

HTTP::RequestParser::RequestParser(void){
}

HTTP::RequestParser::RequestParser(const std::string &sRequest){
    this->parse(sRequest);
}


HTTP::RequestParser::~RequestParser(void){
}

void HTTP::RequestParser::parse(const std::string &sRequest){
    this->clear();

    // Find GET
    int iGetStartIndex    = sRequest.find("GET ");
    if(iGetStartIndex!=utfstring::npos){
        iGetStartIndex+=4;
        // Find next whitespace
        int iGetEndIndex    = sRequest.find_first_of(" \r\n",iGetStartIndex);
        if(iGetEndIndex!=utfstring::npos){
            // a request is found
            this->sFullRequest.assign(sRequest,iGetStartIndex,iGetEndIndex-iGetStartIndex);

            // Find querystring
            int iQuestionMark    = this->sFullRequest.find("?");
            if(iQuestionMark!=utfstring::npos){
                this->sPath.assign(this->sFullRequest.substr(iQuestionMark));
            }else{
                this->sPath.assign(this->sFullRequest);
            }
            this->splitPath();
        }
    }
}
inline void HTTP::RequestParser::splitPath(){
    int iStartSearch(0);

    while(iStartSearch!=std::string::npos){
        int iFirstSlash    = this->sPath.find("/",iStartSearch);
        if(iFirstSlash==std::string::npos){
            if(this->sPath.size()-iStartSearch!=0){
                std::string sMatchPath;
                sMatchPath.assign(this->sPath,iStartSearch,this->sPath.size()-iStartSearch);
                this->aSplitPath.push_back(sMatchPath);
            }
            iStartSearch=std::string::npos;
        }else{
            if(iFirstSlash-iStartSearch!=0){
                std::string sMatchPath;
                sMatchPath.assign(this->sPath,iStartSearch,iFirstSlash-iStartSearch);
                this->aSplitPath.push_back(sMatchPath);
            }
            iStartSearch    = iFirstSlash+1;
        }
    }
}

void HTTP::RequestParser::clear(){
    this->sPath.clear();
    this->sFullRequest.clear();
    this->aSplitPath.clear();
}
