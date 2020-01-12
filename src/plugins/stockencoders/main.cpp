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

#include <core/sdk/IPlaybackRemote.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/IEncoderFactory.h>
#include <set>
#include <algorithm>

#include "shared.h"
#include "LameEncoder.h"
#include "FfmpegEncoder.h"

#include <string>
#include <algorithm>

#ifdef WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#ifdef WIN32
#include <Windows.h>
#include <Objbase.h>
#endif

using namespace musik::core::sdk;

static IEnvironment* environment = nullptr;

static std::set<std::string> supportedFormats = {
    ".mp3",
#ifdef FFMPEG_ENABLED
    "audio/mpeg",
    ".ogg",
    "audio/ogg",
    ".opus",
    ".flac",
    "audio/flac",
    ".alac",
    ".aac",
    "audio/aac",
    ".aac",
    ".m4a",
    ".wma",
    "audio/x-ms-wma",
    ".wv"
#endif
};

static class Plugin : public IPlugin {
    public:
        Plugin() {
        }

        virtual void Release() { }
#if defined(FFMPEG_ENABLED) || defined(WIN32)
        virtual const char* Name() { return "Stock Encoders (MP3 + ffmpeg)"; }
#else
        virtual const char* Name() { return "Stock Encoders (MP3)"; }
#endif
        virtual const char* Version() { return "0.7.0"; }
        virtual const char* Author() { return "clangen"; }
        virtual const char* Guid() { return "d4d13803-a285-4481-ad1e-106131e0d523"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
} plugin;

static class EncoderFactory: public IEncoderFactory {
    public:
        EncoderFactory() {
#ifdef WIN32
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
        }

        virtual void Release() override {
        }

        virtual IEncoder* CreateEncoder(const char* type) override {
            auto lowerType = toLower(type);
            if (isMp3(lowerType)) {
                return new LameEncoder();
            }
#ifdef FFMPEG_ENABLED
            else if (supportedFormats.find(lowerType) != supportedFormats.end()) {
                return new FfmpegEncoder(lowerType);
            }
#endif
            return nullptr;
        }

        virtual bool CanHandle(const char* type) const override {
            return supportedFormats.find(toLower(type)) != supportedFormats.end();
        }

    private:
        std::string toLower(const char* type) const {
            if (!type) {
                return "";
            }

            std::string lowerType(type);
            std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);
            return lowerType;
        }

        bool endsWith(const std::string& s, const std::string& suffix) const {
            return
                s.size() >= suffix.size() &&
                s.rfind(suffix) == (s.size() - suffix.size());
        }

        bool isMp3(const std::string& type) const {
            return endsWith(type, ".mp3") || type == "audio/mpeg";
        }

        bool isOgg(const std::string& type) const {
            return endsWith(type, ".ogg") || type == "audio/ogg" || type == "application/ogg";
        }
} encoderFactory;

extern "C" DLL_EXPORT IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" DLL_EXPORT IEncoderFactory* GetEncoderFactory() {
    return &encoderFactory;
}

extern "C" DLL_EXPORT void SetEnvironment(musik::core::sdk::IEnvironment* env) {
    environment = env;
}

IEnvironment* env() {
    return environment;
}