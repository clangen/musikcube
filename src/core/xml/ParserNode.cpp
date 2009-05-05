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
#include "../pch.hpp"
#include <core/xml/ParserNode.h>
#include <core/xml/Parser.h>

using namespace musik::core::xml;

//////////////////////////////////////////
///\brief
///Contructor
//////////////////////////////////////////
ParserNode::ParserNode()
 :status(0)
 ,parser(NULL)
{
}


//////////////////////////////////////////
///\brief
///Copy a ParserNode
///
///\param copyNode
///node to be copied
///
///\returns
///A reference to *this
///
///The copy method is used when you are writing code like this:
///\code
///while( xml::ParserNode node=parentNode.ChildNode() ){
///}
///\endcode
///In this case, a "node" is created and then copied from the node 
///returned from the parentNode.ChildNode method. This method need to 
///return a reference to itself since the while loop is looking for
///the "operator bool" to see if the loop should continue.
///
///\see
///operator bool
//////////////////////////////////////////
ParserNode& ParserNode::operator=(ParserNode const &copyNode){
    this->node          = copyNode.node;
    this->parentNode    = copyNode.parentNode;
    this->parser        = copyNode.parser;
    this->status        = copyNode.status;
    return *this;
}


//////////////////////////////////////////
///\brief
///Contructor
///
///\param parent
///A pointer to the nodes parent node
///
///This contrutor will hold until the node is read from the socket
///or until an error occurs (like the node should go out of scope)
///
///\see
///ParserNode::ChildNode
//////////////////////////////////////////
ParserNode::ParserNode(const ParserNode *parent)
 : status(0)
{
    this->parentNode    = parent->node;
    this->parser        = parent->parser;

    std::set<std::string> nodeNames;
    this->WaitForNode(nodeNames);
}

//////////////////////////////////////////
///\brief
///Contructor
///
///\param parent
///A pointer to the nodes parent node
///
///\param expectedNode
///A node name to wait for
///
///This contrutor will hold until the expectedNode is read from the socket
///or until an error occurs (like the node should go out of scope)
///
///\see
///ParserNode::ChildNode
//////////////////////////////////////////
ParserNode::ParserNode(const ParserNode *parent,std::string &expectedNode)
 : status(0)
{
    this->parentNode    = parent->node;
    this->parser        = parent->parser;

    std::set<std::string> nodeNames;
    nodeNames.insert(expectedNode);
    this->WaitForNode(nodeNames);
}


//////////////////////////////////////////
///\brief
///Get the first childnode of the current node
//////////////////////////////////////////
ParserNode ParserNode::ChildNode() const{
    return ParserNode(this);
}

//////////////////////////////////////////
///\brief
///Get the first childnode of the current node with
///the expectedNodes name.
//////////////////////////////////////////
ParserNode ParserNode::ChildNode(std::string expectedNode) const{
    return ParserNode(this,expectedNode);
}


//////////////////////////////////////////
///\brief
///Destructor
///
///The destructor will halt until the nodes
///endtag has been retrieved, assuming that
///this is a valid node in the first place
//////////////////////////////////////////
ParserNode::~ParserNode(){
    if(this->node && this->status==1){
        while(this->node->status!=Node::Ended){
            // Wait for node to be ended
            this->parser->ContinueParsing();
        }
    }
}

//////////////////////////////////////////
///\brief
///Get the name of the node
//////////////////////////////////////////
std::string& ParserNode::Name(){
    return this->node->name;
}

//////////////////////////////////////////
///\brief
///Get the text content of the node
//////////////////////////////////////////
std::string& ParserNode::Content(){
    return this->node->content;
}

//////////////////////////////////////////
///\brief
///Wait for all content to be retrieved
///
///What really happens is that the method will wait until
///the nodes end tag has been set to assure that all content
///has been retrieved.
//////////////////////////////////////////
void ParserNode::WaitForContent(){
    if(this->node && this->status==1){
        while( !(this->node->status & Node::Ended) ){
            // Wait for node to be ended
            this->parser->ContinueParsing();
        }
    }
}

//////////////////////////////////////////
///\brief
///Get a reference to the nodes Attributes
///
///The AttributeMap is a std::map that can be used
///like:
///std::string nodeType = node.Attibutes()["type"];
//////////////////////////////////////////
Node::AttributeMap& ParserNode::Attributes(){
    return this->node->attributes;
}

//////////////////////////////////////////
///\brief
///Overload of the bool operator
///
///\returns
///Is this node successfully started
//////////////////////////////////////////
ParserNode::operator bool(){
    return this->status==1;
}

int ParserNode::NodeLevel(){
    int level(0);
    if(this->parentNode){
        return this->parentNode->NodeLevel();
    }
    return level;

}

std::string ParserNode::NodeParentsPath(){
    std::string nodeParents;
    if(this->parentNode){
        return this->parentNode->NodeLevelPath();
    }
    return nodeParents;
}

void ParserNode::WaitForNode(const std::set<std::string> &nodeNames){
    // Wait for the node
    std::string nodeParentPath(this->NodeParentsPath());

    while(this->status==0){

        if(this->parser->exit){
            this->status    = -1;
        }else{

            // Parse until next stop
            this->parser->ContinueParsing();

            // First of all, check if the nodeParentPath is a substring of the parsers current node level
            if( this->parser->CurrentNodeLevelPath().compare(0,nodeParentPath.size(),nodeParentPath)!=0 ){
                // Node has not turned up, fail
                this->status=-1;
            }else{
                // Check if this is the expected EventType
                if(this->parser->currentEventType==Parser::NodeStart){
                    
                    // Is the "level" the right one?
                    // This is a check so that the parser isn't a level down from the expected
                    if(this->parser->level-1==this->NodeLevel()){

                        // Is this an expected node name
                        if(nodeNames.empty()){
                            // Success
                            this->node      = this->parser->LastNode();
                            this->status    = 1;
                        }else{
                            Node::Ptr lastNode  = this->parser->LastNode();
                            if(lastNode){
                                if(nodeNames.find(lastNode->name)!=nodeNames.end()){
                                    // Success
                                    this->node      = lastNode;
                                    this->status    = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

