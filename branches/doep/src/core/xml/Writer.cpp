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
#include <core/xml/Writer.h>
#include <core/xml/WriterNode.h>


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
Writer::Writer(boost::asio::ip::tcp::socket *socket)
 :socket(socket)
 ,exit(false)
{
    this->writer    = this;

}

//////////////////////////////////////////////////////////////////////////////

Writer::~Writer(void){
}

//////////////////////////////////////////////////////////////////////////////

void Writer::Exit(){
    this->exit  = true;
}

//////////////////////////////////////////////////////////////////////////////
void Writer::Send(){

    // If no node is set, set the root node
    if(!this->currentWritingNode && this->node){
        this->currentWritingNode = this->node;
    }

    bool keepSending(true);
    while(keepSending){

        keepSending=false;

        // 1. Send start tag if:
        //      * start is not send before
        //      * the node is started, and has childnodes
        //      * if it's ended
        if( !(this->currentWritingNode->status|Node::Status::StartSend) &&
            (
                (this->currentWritingNode->status|Node::Status::Started && !this->currentWritingNode->childNodes.empty())
                ||
                (this->currentWritingNode->status|Node::Status::Ended)
            )
           ){
            // OK, lets send the start tag
            this->sendBuffer.append("<"+this->currentWritingNode->name);

            // append the attributes
            for(Node::AttributeMap::iterator attribute=this->currentWritingNode->attributes.begin();attribute!=this->currentWritingNode->attributes.end();++attribute){
               this->sendBuffer.append(" "+attribute->first+"=\""+attribute->second+"\"");
            }
            this->sendBuffer.append(">");

            // Set the node to StartSend
            this->currentWritingNode->status |= Node::Status::StartSend;

            // Lets see if the node has any children to continue with
            if(!keepSending && !this->currentWritingNode->childNodes.empty()){
                this->currentWritingNode = this->currentWritingNode->childNodes.front();
                keepSending = true;
            }
        }


        // 2. Lets send endtag if:
        //      * keepSending == false (currentWritingNode has not been altered)
        //      * StartTag has been send
        //      * Ended is set
        //      * There are no childnodes left
        if(!keepSending &&
            this->currentWritingNode->status|Node::Status::StartSend &&
            this->currentWritingNode->status|Node::Status::Ended &&
            this->currentWritingNode->childNodes.empty()
            ){

            // Send the content and end tag
            this->sendBuffer.append(this->currentWritingNode->content);
            this->sendBuffer.append("</"+this->currentWritingNode->name+">");

            // Set to send
            this->currentWritingNode->status    |= Node::Status::EndSend;

            // Remove from parent node
            this->currentWritingNode->RemoveFromParent();

            // And lets step down the currentWritingNode to parent
            this->currentWritingNode    = this->currentWritingNode->parent;
            keepSending = true;
        }
    }

    // Time to send the buffer
    boost::asio::write(*(this->socket),boost::asio::buffer(this->sendBuffer));

}


