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
    this->node.reset();
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

    std::string sendBuffer;

    bool keepSending(true);
    while(keepSending && this->currentWritingNode){

        keepSending=false;

        // If the node has already been send, and has childnodes, set currentWritingNode to the first childnode
        if(this->currentWritingNode->status&Node::Status::StartSend && !this->currentWritingNode->childNodes.empty() ){
            this->currentWritingNode    = this->currentWritingNode->childNodes.front();
        }

        // 1. Send start tag if:
        //      * start is not send before
        //      * the node is started, and has childnodes
        //      * if it's ended
        Node::Ptr node(this->currentWritingNode);
        int status(node->status);
        bool noChildren(node->childNodes.empty());

        if( !(status&Node::Status::StartSend) &&
            (
                (status&Node::Status::Started && !noChildren)
                ||
                (status&Node::Status::Ended)
            )
           ){

            if(node!=this->node){

                // OK, lets send the start tag
                sendBuffer.append("<"+node->name);

                // append the attributes
                for(Node::AttributeMap::iterator attribute=node->attributes.begin();attribute!=node->attributes.end();++attribute){
                   sendBuffer.append(" "+attribute->first+"=\""+attribute->second+"\"");
                }
                sendBuffer.append(">");
            }

            // Set the node to StartSend
            node->status |= Node::Status::StartSend;

            // Lets see if the node has any children to continue with
            if(!keepSending && !noChildren){
                this->currentWritingNode = node->childNodes.front();
                keepSending = true;
            }
        }


        // 2. Lets send endtag if:
        //      * keepSending == false (currentWritingNode has not been altered)
        //      * StartTag has been send
        //      * Ended is set
        //      * There are no childnodes left
        if(this->currentWritingNode){
            node        = this->currentWritingNode;
            status      = node->status;
            noChildren  = node->childNodes.empty();

            if(!keepSending &&
                status&Node::Status::StartSend &&
                status&Node::Status::Ended &&
                noChildren
                ){

                if(node!=this->node){   // Do not send root node
                    // Send the content and end tag
                    sendBuffer.append(node->content);
                    sendBuffer.append("</"+node->name+">");
                }

                // Set to send
                node->status    |= Node::Status::EndSend;

                // Remove from parent node
                node->RemoveFromParent();

                // And lets step down the currentWritingNode to parent
                this->currentWritingNode    = node->parent;
                keepSending = true;
            }
        }
    }

    // Time to send the buffer
    if(!sendBuffer.empty()){
        boost::asio::write(*(this->socket),boost::asio::buffer(sendBuffer));
        sendBuffer.clear();
    }

}


