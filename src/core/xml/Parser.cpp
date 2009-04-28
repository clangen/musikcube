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
#include <core/xml/Parser.h>
#include <core/xml/ParserNode.h>
#include <expat/expat.h>
#include <boost/algorithm/string/split.hpp>

using namespace musik::core::xml;

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///Constructor
///
///\param socket
///pointer to boost::asio socket to be used by the Parser
///
///The Parser will continously parse XML and 
///read from the socket when there is no buffer
///left in the parser.
//////////////////////////////////////////
Parser::Parser(IReadSupplier *supplier)
 :level(0)
 ,supplier(supplier)
 ,xmlParser(NULL)
 ,xmlParserStatus(XML_STATUS_OK)
 ,currentEventType(0)
 ,exit(false)
{
    // Set node stuff
    this->parser        = this;
	this->InitExpat();
}

void Parser::InitExpat(){
	if(this->xmlParser!=NULL){
		XML_ParserFree(this->xmlParser);
	}
    this->xmlParser    = XML_ParserCreate(NULL);
    XML_SetUserData(this->xmlParser,this);
    XML_SetElementHandler(this->xmlParser,&Parser::OnElementStart,&Parser::OnElementEnd);
    XML_SetCharacterDataHandler(this->xmlParser,&Parser::OnContent);

	this->xmlParserStatus	= XML_STATUS_OK;
	this->currentEventType	= 0;
}

//////////////////////////////////////////////////////////////////////////////

Parser::~Parser(void){
    XML_ParserFree(this->xmlParser);
}

//////////////////////////////////////////////////////////////////////////////

void Parser::OnElementStart(void *thisobject,const char *name, const char **atts){
    ((Parser*)thisobject)->OnElementStartReal(name,atts);
}
void Parser::OnElementStartReal(const char *name, const char **atts){
    this->xmlFound  = true;
    this->level++;
    // First, lets create the new Node and add to the nodeLevels
    Node::Ptr node(new Node());
    node->name    = name;

    // fix attributes
    for (int i(0);atts[i];i+=2){
        node->attributes[atts[i]] = atts[i + 1];
    }

    // Set the parent node
    if(!this->currentNodeLevels.empty()){
        node->parent    = this->currentNodeLevels.back();
    }
    this->currentNodeLevels.push_back(node);


    this->currentEventType  = NodeStart;
    XML_StopParser(this->xmlParser,true);
}

//////////////////////////////////////////////////////////////////////////////

void Parser::OnElementEnd(void *thisobject,const char *name){
    ((Parser*)thisobject)->OnElementEndReal(name);
}

void Parser::OnElementEndReal(const char *name){
    this->xmlFound  = true;
    this->level--;
    if(this->currentNodeLevels.size()>0){
        if(this->currentNodeLevels.back()->name == name){

            this->currentNodeLevels.back()->status   |= Node::Ended;
            this->currentNodeLevels.pop_back();

            this->currentEventType  = NodeEnd;
            XML_StopParser(this->xmlParser,true);

        }else{
            // Wrong endtag expected, mallformated input
            this->Exit();
            XML_StopParser(this->xmlParser,true);
        }
    }else{
        // below root level, mallformated input
        XML_StopParser(this->xmlParser,true);
		this->InitExpat();
    }
}

//////////////////////////////////////////////////////////////////////////////

void Parser::OnContent(void *thisobject,const char *content,int length){
    ((Parser*)thisobject)->OnContentReal(content,length);
}

void Parser::OnContentReal(const char *content,int length){
    if(this->currentNodeLevels.size()>0){
        this->currentNodeLevels.back()->content.append(content,length);
    }
}

//////////////////////////////////////////////////////////////////////////////

void Parser::ContinueParsing(){
    this->xmlFound  = false;
	std::string errorstring;
	while( !this->xmlFound && !this->exit){
        switch(this->xmlParserStatus){
            case XML_STATUS_SUSPENDED:
		        this->xmlParserStatus	= XML_ResumeParser(this->xmlParser);
				if(this->xmlParserStatus==XML_STATUS_ERROR){
					errorstring	= XML_ErrorString(XML_GetErrorCode(this->xmlParser));
					errorstring.append(" ");
				}
		        break;
            case XML_STATUS_OK:
                if(this->supplier->Exited()){
                    this->Exit();
                    return;
                }

				{
					// create a temporary buffer to be able to remove null characters
					std::string tempBuffer(this->nextBuffer);
					this->nextBuffer.clear();

					if(tempBuffer.empty()){
						if(!this->supplier->Read()){
							return;
						}
						tempBuffer.append(this->supplier->Buffer(),this->supplier->BufferSize());
					}

					static std::string nullCharacter("\0",1);
					std::string::size_type nullPosition(tempBuffer.find(nullCharacter));
					if(nullPosition!=std::string::npos){
						// There is a null char in buffer, lets append to the nextBuffer
						this->nextBuffer	= tempBuffer.substr(nullPosition+1);
						tempBuffer.resize(nullPosition);
					}


	//                this->xmlParserStatus	= XML_Parse(this->xmlParser,this->supplier->Buffer(),(int)this->supplier->BufferSize(),0);
					this->xmlParserStatus	= XML_Parse(this->xmlParser,tempBuffer.c_str(),(int)tempBuffer.size(),0);
				}
		        break;
            case XML_STATUS_ERROR:
                this->Exit();
                break;
        }
    }
	// check if we need to restart the parser
	if(this->currentNodeLevels.empty()){
		this->InitExpat();
	}
}

void Parser::Exit(){
    this->exit  = true;
    for(std::vector<Node::Ptr>::iterator node=this->currentNodeLevels.begin();node!=this->currentNodeLevels.end();++node){
        (*node)->status  = Node::Ended;
    }
}


//////////////////////////////////////////////////////////////////////////////

std::string Parser::CurrentNodeLevelPath(bool getParent){
    std::string nodeLevels;
    bool first(true);
    int i(0);
    for(std::vector<Node::Ptr>::iterator node=this->currentNodeLevels.begin();node!=this->currentNodeLevels.end();++node){
        ++i;
        if(!getParent || this->currentNodeLevels.size()!=i){
            if(first){
                first=false;
                nodeLevels += (*node)->name;
            }else{
                nodeLevels += "/"+(*node)->name;
            }
        }
    }
    return nodeLevels;
}

//////////////////////////////////////////////////////////////////////////////

Node::Ptr Parser::LastNode(){
    if(this->currentNodeLevels.empty()){
        return Node::Ptr();
    }
    return this->currentNodeLevels.back();
}


