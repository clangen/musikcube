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

#include <core/sdk/constants.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/sdk/IDebug.h>
#include <core/sdk/ITagReader.h>
#include <libopenmpt/libopenmpt.h>

extern "C" {
    #include "OpenMptDecoder.h"
}

#ifdef WIN32
#include <Windows.h>
#define DLLEXPORT __declspec(dllexport)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return true;
}
#else
#define DLLEXPORT
#endif

using namespace musik::core::sdk;

IDebug* debug = nullptr;

class OpenMptPlugin: public IPlugin {
    public:
        virtual void Release() { delete this; };
        virtual const char* Name() { return "libopenmpt IDecoder"; }
        virtual const char* Version() { return "0.1.0"; }
        virtual const char* Author() { return "clangen"; }
        virtual const char* Guid() { return "c367f42f-389a-4378-966c-1f96dad6a66a"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
};

static bool canHandle(const char* type) {
    const char* actualType = strlen(type) && type[0] == '.' ? &type[1] : type;
    return openmpt_is_extension_supported(actualType);
}

class OpenMptDecoderFactory: public IDecoderFactory {
    public:
        virtual IDecoder* CreateDecoder() override {
            return new OpenMptDecoder();
        }

        virtual void Release() override {
            delete this;
        }

        virtual bool CanHandle(const char* type) const override {
            return canHandle(type);
        }
};

class OpenMptTgReader : public ITagReader {
    public:
        virtual bool Read(const char *uri, musik::core::sdk::ITagStore *target) override {
            return false;
        }

        virtual bool CanRead(const char *extension) override {
            return canHandle(extension);
        }

        virtual void Release() override {
            delete this;
        }
};

extern "C" DLLEXPORT IPlugin* GetPlugin() {
    return new OpenMptPlugin();
}

extern "C" DLLEXPORT IDecoderFactory* GetDecoderFactory() {
    return new OpenMptDecoderFactory();
}

extern "C" DLLEXPORT musik::core::sdk::ITagReader* GetTagReader() {
    return new OpenMptTgReader();
}

extern "C" DLLEXPORT void SetDebug(IDebug* debug) {
    ::debug = debug;
}
