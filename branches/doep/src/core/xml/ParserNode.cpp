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
#include <core/xml/ParserNode.h>
#include <core/xml/Parser.h>

using namespace musik::core::xml;
    
ParserNode::ParserNode(ParserNode &parent){
    this->parentNode    = parent.node;
    this->parser        = parent.parser;
    this->success       = this->parser->WaitForNode(this);
}

ParserNode::ParserNode(ParserNode &parent,std::string expectedNode){
    this->parentNode    = parent.node;
    this->parser        = parent.parser;
    this->success       = this->parser->WaitForNode(this,expectedNode);
}

ParserNode::~ParserNode(){
    if(this->success && !this->node->ended){    
        // Wait for node to be ended

    }
}

std::string& ParserNode::Name(){
    return this->node->name;
}

Node::AttributeMap& ParserNode::Attributes(){
    return this->node->attributes;
}

ParserNode::operator bool(){
    return this->success;
}

std::list<std::string> ParserNode::NodeParents(){
    std::list<std::string> nodeParents;
    if(this->parentNode){
        return this->parentNode->NodeLevel();
    }
    return nodeParents;
}
