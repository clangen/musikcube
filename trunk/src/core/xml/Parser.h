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
#include <sigslot/sigslot.h>

#include <vector>
#include <set>

#include <core/xml/IReadSupplier.h>
#include <core/xml/Node.h>
#include <core/xml/ParserNode.h>

//////////////////////////////////////////////////////////////////////////////
// Forward
typedef struct XML_ParserStruct *XML_Parser;


namespace musik{ namespace core{ namespace xml{
    // Forward
    class MUSIK_EXPORT ParserNode;
} } }
//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace xml{

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///Parser class to parse xml continously from a socket stream
///
///The Parser and the ParserNode contiously parse from the
///socket provided in the constructor like this
///\code
///xml::Parser parser(&socket);
///
///\/\/To wait for a specific xml node to arrive
///if(xml::ParserNode rootNode = parser.ChildNode("musik") ){
///    \/\/Make a loop, waiting for any xml node to arrive that's inside the rootNode
///    while( xml::ParserNode node = rootNode.ChildNode() ){
///        // The Nodes can be used to access the tags attributes and content
///        std::cout << "Tag: " << node.Name() << std::endl;
///        std::cout << "Attribute[type]: " << node.Attributes()["type"] << std::endl;
///        \/\/ Although, if you are to use the content of the node, you need to make sure that the content is read
///        node.WaitForContent();
///        std::cout << "Content: " << node.Content() << std::endl;
///    }
///}
///\endcode
///
///\see
///musik::core::xml::ParserNode
//////////////////////////////////////////
class MUSIK_EXPORT Parser : public ParserNode{
    public:
        Parser(IReadSupplier *supplier);
        ~Parser();

    private:
        IReadSupplier *supplier;
		// XML Parser status
        XML_Parser xmlParser;
		int xmlParserStatus;

		// XML specific info
        int level;
        std::vector<Node::Ptr> currentNodeLevels;

    private:

        static void OnElementStart(void *thisobject,const char *name, const char **atts);
        void OnElementStartReal(const char *name, const char **atts);
        static void OnElementEnd(void *thisobject,const char *name);
        void OnElementEndReal(const char *name);
        static void OnContent(void *thisobject,const char *content,int length);
        void OnContentReal(const char *content,int length);

		void InitExpat();

    private:
        friend class ParserNode;

        bool exit;
        void Exit();

        typedef enum {
            NodeStart=1,
            NodeEnd=2,
            Content=3
        } EventTypes;

        Node::Ptr LastNode();
		void ContinueParsing();
        std::string CurrentNodeLevelPath(bool getParent=false);

        int currentEventType;

        bool xmlFound;

		std::string nextBuffer;

};

//////////////////////////////////////////////////////////////////////////////
} } }

