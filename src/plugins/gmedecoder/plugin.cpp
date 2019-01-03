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

#include "Constants.h"
#include <core/sdk/constants.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/IDecoderFactory.h>
#include "GmeDecoder.h"
#include "GmeIndexerSource.h"

#ifdef WIN32
    BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
        return true;
    }
#endif

static inline bool supported(const std::string& s) {
    for (auto& ext : FORMATS) {
        if (s.rfind(ext) == s.size() - ext.size()) {
            return true;
        }
    }
    return false;
}

class GmePlugin : public musik::core::sdk::IPlugin {
    public:
        virtual void Release() { delete this; };
        virtual const char* Name() { return PLUGIN_NAME.c_str(); }
        virtual const char* Version() { return "0.1.0"; }
        virtual const char* Author() { return "clangen"; }
        virtual const char* Guid() { return "2c4eee19-6585-4984-a631-b52ff7d6d564"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
};

class GmeDecoderFactory : public musik::core::sdk::IDecoderFactory {
    public:
        GmeDecoderFactory() {
        }

        ~GmeDecoderFactory() {
        }

        virtual musik::core::sdk::IDecoder* CreateDecoder() override {
            return new GmeDecoder();
        }

        virtual void Release() override {
            delete this;
        }

        virtual bool CanHandle(const char* type) const override {
            return supported(std::string(type));
        }
};

extern "C" DLLEXPORT musik::core::sdk::IPlugin* GetPlugin() {
    return new GmePlugin();
}

extern "C" DLLEXPORT musik::core::sdk::IDecoderFactory* GetDecoderFactory() {
    return new GmeDecoderFactory();
}

extern "C" DLLEXPORT IIndexerSource* GetIndexerSource() {
    return new GmeIndexerSource();
}