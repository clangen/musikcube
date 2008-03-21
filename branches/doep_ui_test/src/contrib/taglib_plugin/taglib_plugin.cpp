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


#include "stdafx.h"
#include "TagReaderTaglib.h"
#include "core/IPlugin.h"
#include <id3v2framefactory.h>

#define DLLEXPORT __declspec(dllexport)


//////////////////////////////////////////
///\brief
///Class for fixing leaking TagLib::ID3v2::FrameFactory without changing TagLib code.
//////////////////////////////////////////
class TaglibBugFix : TagLib::ID3v2::FrameFactory{
    public:
        ~TaglibBugFix(){

        };
};


BOOL APIENTRY DllMain( HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved){
    if(ul_reason_for_call==DLL_PROCESS_DETACH){
        TaglibBugFix *ff    = (TaglibBugFix*)TagLib::ID3v2::FrameFactory::instance();
        delete ff;
    }
    return TRUE;
}

//TagReaderTaglib tagreader;

class TaglibPlugin : public musik::core::IPlugin
{
    void Destroy() { delete this; };

	const wchar_t* Name(){
		return _T("Taglib 1.5 plugin");
	};

	const wchar_t* Version(){
		return _T("0.1");
	};

	const wchar_t* Author(){
		return _T("Daniel Önnerby");
	};

};

extern "C"{
	DLLEXPORT musik::core::Plugin::IMetaDataReader* GetMetaDataReader(){
		return new TagReaderTaglib();
	}

	DLLEXPORT musik::core::IPlugin* GetPlugin(){
		return new TaglibPlugin();
	}
}
