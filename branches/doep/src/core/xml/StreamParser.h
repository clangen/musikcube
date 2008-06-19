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
#include <expat/expat.h>
#include <sigslot/sigslot.h>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace xml{

//////////////////////////////////////////////////////////////////////////////

class StreamParser{
    public:
        StreamParser(void);
        ~StreamParser(void);

        bool Parse(std::string &xml);

        typedef std::map<std::string,std::string> AttributeMap;
        
        sigslot::signal2<const char*,AttributeMap&> RootNodeStart;
        sigslot::signal1<const char*> RootNodeEnd;

        sigslot::signal2<const char*,AttributeMap&> NodeStart;
        sigslot::signal1<const char*> NodeContent;
        sigslot::signal1<const char*> NodeEnd;


    private:
        XML_Parser parser;

        int level;
        std::vector<std::string> tagLevels;
        
        

        static void OnElementStart(void *thisobject,const char *name, const char **atts);
        void OnElementStartReal(const char *name, const char **atts);
        static void OnElementEnd(void *thisobject,const char *name);
        void OnElementEndReal(const char *name);
        static void OnContent(void *thisobject,const char *content,int length);
        void OnContentReal(const char *content,int length);


};

//////////////////////////////////////////////////////////////////////////////
} } }

