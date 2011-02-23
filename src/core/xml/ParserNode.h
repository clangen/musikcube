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
#pragma once

#include <core/config.h>
#include <core/xml/Node.h>
#include <boost/utility.hpp>
#include <set>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace xml{

// Forward
class  Parser;


//////////////////////////////////////////
///\brief
///ParserNode represent a xml-node in a DOM tree.
///
///For example look at musik::core::xml::Parser
///
///\see
///musik::core::xml::Parser
//////////////////////////////////////////
class  ParserNode {
    public:

        ParserNode();
        ~ParserNode();

        ParserNode& operator=(ParserNode const &copyNode);

        ParserNode ChildNode() const;
        ParserNode ChildNode(std::string expectedNode) const;

        std::string& Name();
        std::string& Content();
        Node::AttributeMap& Attributes();

        void WaitForContent();

        operator bool();

    protected:
        friend class Parser;

        ParserNode(const ParserNode *parent);
        ParserNode(const ParserNode *parent,std::string &expectedNode);

        void WaitForNode(const std::set<std::string> &nodeNames);

        std::string NodeParentsPath();
        int NodeLevel();

        Node::Ptr node;
        Node::Ptr parentNode;
        Parser *parser;
        int status;

};

//////////////////////////////////////////////////////////////////////////////
} } }
