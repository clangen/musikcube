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
#include <core/xml/WriterNode.h>
#include <core/xml/Writer.h>

using namespace musik::core::xml;

WriterNode::WriterNode(WriterNode &parentNode,std::string name)
:writer(parentNode.writer)
,node(new Node(parentNode.node))
{
    this->parentNode    = parentNode.node;
    this->node->name    = name;

    if(parentNode.node){
        parentNode.node->childNodes.push_back(this->node);
        // if a childnode has been added to a node, it should be set to started
        parentNode.node->status |= Node::Started;
    }

    this->writer->Send();

}

WriterNode::WriterNode()
:writer(NULL)
,node(new Node())
{
}


//////////////////////////////////////////
///\brief
///Destructor
///
//////////////////////////////////////////
WriterNode::~WriterNode(){
    if(this->node){
        this->node->status  |= Node::Started | Node::Ended;
        if(this!=this->writer){
            this->writer->Send();
        }
    }
}

//////////////////////////////////////////
///\brief
///Get the name of the node
//////////////////////////////////////////
std::string& WriterNode::Name(){
    return this->node->name;
}

//////////////////////////////////////////
///\brief
///Get the text content of the node
//////////////////////////////////////////
std::string& WriterNode::Content(){
    return this->node->content;
}


//////////////////////////////////////////
///\brief
///Get a reference to the nodes Attributes
///
///The AttributeMap is a std::map that can be used
///like:
///node.Attibutes()["type"] = "tjobba";
//////////////////////////////////////////
Node::AttributeMap& WriterNode::Attributes(){
    return this->node->attributes;
}

void WriterNode::Flush(){
    if(this->writer){
        this->writer->supplier->Flush();
    }
}

