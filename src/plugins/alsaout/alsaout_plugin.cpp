//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "pch.h"

#include <core/sdk/constants.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/IOutput.h>

#include "AlsaOut.h"

class AlsaPlugin : public musik::core::sdk::IPlugin {
    public:
        virtual void Release() override { delete this; }
        virtual const char* Name() override { return "AlsaOut IOutput"; }
        virtual const char* Version() override { return "0.5.0"; }
        virtual const char* Author() override { return "Julian Cromarty, clangen"; }
        virtual const char* Guid() override { return "668a75e6-1816-4c75-a361-a9d48906f23f"; }
        virtual bool Configurable() override { return false; }
        virtual void Configure() override { }
        virtual void Reload() override { }
        virtual int SdkVersion() override { return musik::core::sdk::SdkVersion; }
};

extern "C" musik::core::sdk::IPlugin* GetPlugin() {
	return new AlsaPlugin();
}

extern "C" musik::core::sdk::IOutput* GetAudioOutput() {
	return new AlsaOut();
}
