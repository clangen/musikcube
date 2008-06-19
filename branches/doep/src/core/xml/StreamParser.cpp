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
#include <core/xml/StreamParser.h>

using namespace musik::core::xml;

//////////////////////////////////////////////////////////////////////////////

StreamParser::StreamParser(void)
:level(0)
,parser(NULL)
{
    this->parser    = XML_ParserCreate(NULL);
    XML_SetUserData(this->parser,this);
    XML_SetElementHandler(this->parser,&StreamParser::OnElementStart,&StreamParser::OnElementEnd);
    XML_SetCharacterDataHandler(this->parser,&StreamParser::OnContent);
}

StreamParser::~StreamParser(void){
    XML_ParserFree(this->parser);
}

bool StreamParser::Parse(std::string &xml){
    if(XML_Parse(this->parser,xml.c_str(),xml.length(),0)==XML_Status::XML_STATUS_OK){
        return true;
    }
    return false;
}

void StreamParser::OnElementStart(void *thisobject,const char *name, const char **atts){
    ((StreamParser*)thisobject)->OnElementStartReal(name,atts);
}
void StreamParser::OnElementStartReal(const char *name, const char **atts){

    // fix attributes
    AttributeMap attributes;
    for (int i(0);atts[i];i+=2){
        attributes[atts[i]] = atts[i + 1];
    }


    this->tagLevels.push_back(name);
    this->level++;
    if(this->level==1){
        // first rootnode
        this->RootNodeStart(name,attributes);
    }else{
        this->NodeStart(name,attributes);
    }
}

void StreamParser::OnElementEnd(void *thisobject,const char *name){
    ((StreamParser*)thisobject)->OnElementEndReal(name);
}
void StreamParser::OnElementEndReal(const char *name){
    if(this->tagLevels.back()==name){
        this->tagLevels.pop_back();
        this->level--;
        if(this->level==0){
            this->RootNodeEnd(name);
        }else{
            if(this->level<0){
                throw;
            }
            this->NodeEnd(name);
        }
    }else{
        throw;
    }
}

void StreamParser::OnContent(void *thisobject,const char *content,int length){
    ((StreamParser*)thisobject)->OnContentReal(content,length);
}

void StreamParser::OnContentReal(const char *content,int length){
}

