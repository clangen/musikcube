//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include "HttpDataStreamFactory.h"

#include <core/sdk/constants.h>
#include <core/sdk/IPlugin.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#ifdef WIN32
    BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
        return true;
    }

    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT
#endif

static class HttpDataStreamPlugin : public musik::core::sdk::IPlugin {
    public:
        HttpDataStreamPlugin() {
            /* enable utf8 filesystem (required in windows, maybe not macos/linux */
            std::locale locale = std::locale();
            std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
            boost::filesystem::path::imbue(utf8Locale);
        }

        virtual void Release() { };
        virtual const char* Name() { return "HTTP IDataStream"; }
        virtual const char* Version() { return "0.1.0"; }
        virtual const char* Author() { return "clangen"; }
        virtual const char* Guid() { return "b153adad-ee98-4331-ad32-4ff7f34828cd"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
} plugin;

extern "C" DLLEXPORT IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" DLLEXPORT IDataStreamFactory* GetDataStreamFactory() {
    return new HttpDataStreamFactory();
}
