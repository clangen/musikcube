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
#include "Node.h"
#include <core/xml/Parser.h>

using namespace musik::core::xml;
    
Node::Node()
 :status(0)
{

}

Node::Node(Ptr parent)
 :status(0)
 ,parent(parent)
{
}


Node::~Node(void){
    if(this->parent){
        // Erase in parents childnodes
        for(ChildNodes::iterator node=this->parent->childNodes.begin();node!=this->parent->childNodes.end();){
            if( node->get()==this ){
                node    = this->parent->childNodes.erase(node);
            }else{
                ++node;
            }
        }
    }
}

std::string Node::NodeLevelPath(){
    std::string nodeLevels(this->name);

    Ptr currentNode   = this->parent;
    while(currentNode){
        nodeLevels  = currentNode->name + "/" + nodeLevels;
        currentNode = currentNode->parent;
    }
    return nodeLevels;
}

int Node::NodeLevel(){
    int level(1);
    Ptr currentNode   = this->parent;
    while(currentNode){
        level++;
        currentNode = currentNode->parent;
    }
    return level;

}

void Node::RemoveFromParent(){
    if(this->parent){
        for(Node::ChildNodes::iterator node=this->parent->childNodes.begin();node!=this->parent->childNodes.end();){
            if( node->get()==this ){
                node = this->parent->childNodes.erase(node);
            }else{
                ++node;
            }
        }
    }
}


