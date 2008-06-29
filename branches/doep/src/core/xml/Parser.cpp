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


using namespace musik::core::xml;

//////////////////////////////////////////////////////////////////////////////

Parser::Parser(boost::asio::ip::tcp::socket *socket)
 :level(0)
 ,xmlParser(NULL)
 ,socket(socket)
 ,xmlParserStatus(XML_Status::XML_STATUS_OK)
 ,readBufferLength(0)
 ,setNode(NULL)
{
    this->xmlParser    = XML_ParserCreate(NULL);
    XML_SetUserData(this->xmlParser,this);
    XML_SetElementHandler(this->xmlParser,&Parser::OnElementStart,&Parser::OnElementEnd);
//    XML_SetCharacterDataHandler(this->xmlParser,&Parser::OnContent);

}

Parser::~Parser(void){
    XML_ParserFree(this->xmlParser);
}


void Parser::OnElementStart(void *thisobject,const char *name, const char **atts){
    ((Parser*)thisobject)->OnElementStartReal(name,atts);
}
void Parser::OnElementStartReal(const char *name, const char **atts){

    // First, lets create the new Node and add to the nodeLevels
    Node::Ptr node(new Node());
    node->name    = name;

    // fix attributes
    for (int i(0);atts[i];i+=2){
        node->attributes[atts[i]] = atts[i + 1];
    }

    this->currentNodeLevels.push_back(node);

    // Second, check to see if this is the expected node
    if(this->CheckExpected(AWaitingTypes::NodeStart)){
        this->continueParsing   = false;
        XML_StopParser(this->xmlParser,true);
    }
}

void Parser::OnElementEnd(void *thisobject,const char *name){
    ((Parser*)thisobject)->OnElementEndReal(name);
}
void Parser::OnElementEndReal(const char *name){
    if(this->currentNodeLevels.size()>0){
        if(this->currentNodeLevels.back()->name == name){
            Node::Ptr endNode   = this->currentNodeLevels.back();
            this->currentNodeLevels.pop_back();

            endNode->ended   = true;

            if(this->CheckExpected(AWaitingTypes::NodeEnd)){
                this->continueParsing   = false;
                XML_StopParser(this->xmlParser,true);
            }

        }else{
            // Wrong endtag expected, mallformated input
            throw;
        }
    }else{
        // below root level, mallformated input
        throw;
    }
}

bool Parser::CheckExpected(int typeOfEvent){
    // If this is expected
    if(this->awaitingType==typeOfEvent){
        // Right type of event

        if(this->setNode){
            // There is a node that is waiting

            if(typeOfEvent==AWaitingTypes::NodeStart){
                // Get the current parent node
                std::list<std::string> nodeParents(this->NodeLevel());
                nodeParents.pop_back();

                // Check if the node parents are the same as the setNode
                if(nodeParents==this->setNode->NodeParents()){
                    // Node parents are the same
                    // check if the expected nodes is here
                    if(this->awaitingNodes.empty()){
                        // any node name is accepted :)
                        this->setNode->node = this->currentNodeLevels.back();
                        return true;
                    }else{
                        // Check for valid node name
                        if(this->awaitingNodes.find(this->currentNodeLevels.back()->name)!=this->awaitingNodes.end()){
                            // node is amongst the expected
                            this->setNode->node = this->currentNodeLevels.back();
                            return true;
                        }
                    }
                }
            }else if(typeOfEvent==AWaitingTypes::NodeEnd){
                if(this->NodeLevel()==this->setNode->NodeParents()){
                    // Expected node has ended. Stop the parser
                    return true;
                }
            }
        }
    }

    // Wrong expected event
    return false;
    
}


/*
void Parser::OnContent(void *thisobject,const char *content,int length){
    ((Parser*)thisobject)->OnContentReal(content,length);
}

void Parser::OnContentReal(const char *content,int length){
}*/


bool Parser::WaitForNode(ParserNode *setNode){
    this->awaitingType  = AWaitingTypes::NodeStart;
    this->awaitingNodeLevels    = setNode->NodeParents();
    this->awaitingNodes.clear();
    this->setNode   = setNode;
    this->setNodeSuccess    = false;
    this->ContinueParsing();
    this->setNode   = NULL;
    return this->setNodeSuccess;
}

bool Parser::WaitForNode(ParserNode *setNode,std::string &expectedNode){
    this->awaitingType  = AWaitingTypes::NodeStart;
    this->awaitingNodeLevels    = setNode->NodeParents();
    this->awaitingNodes.clear();
    this->awaitingNodes.insert(expectedNode);
    this->setNode   = setNode;
    this->setNodeSuccess    = false;
    this->ContinueParsing();
    this->setNode   = NULL;
    return this->setNodeSuccess;
}
/*
bool Parser::WaitForNode(Node &setNode,std::set<std::string> &expectedNodes){
    this->awaitingType  = AWaitingTypes::Node;
    this->awaitingNodeLevels    = setNode.NodeLevel();
    this->awaitingNodes    = expectedNodes;
    this->setNode   = &setNode;
    this->ContinueParsing();
    this->setNode   = NULL;
}
*/
void Parser::ReadFromSocket(){
    boost::system::error_code error;
    this->readBufferLength  = this->socket->read_some(boost::asio::buffer(this->readBuffer),error);

    if(error){
        // Connection closed or some other error occured
        throw;
    }

}

void Parser::ContinueParsing(){
	this->continueParsing		= true;

	while(this->continueParsing){
		switch(this->xmlParserStatus){
			case XML_Status::XML_STATUS_SUSPENDED:
				this->xmlParserStatus	= XML_ResumeParser(this->xmlParser);
				break;
			case XML_Status::XML_STATUS_OK:
				this->ReadFromSocket();
				this->xmlParserStatus	= XML_Parse(this->xmlParser,this->readBuffer.c_array(),this->readBufferLength,0);
				break;
            case XML_Status::XML_STATUS_ERROR:
                throw;
                break;
		}
	}
}

std::list<std::string> Parser::NodeLevel(){
    std::list<std::string> nodeLevels;
    for(std::vector<Node::Ptr>::iterator node=this->currentNodeLevels.begin();node!=this->currentNodeLevels.end();++node){
        nodeLevels.push_back((*node)->name);
    }
    return nodeLevels;
}

