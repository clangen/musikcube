//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include "FfmpegDecoder.h"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <set>
#include <map>

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#ifdef WIN32
#include <Windows.h>
#include <Objbase.h>

    BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
        return true;
    }
#endif

class FfmpegPlugin : public musik::core::sdk::IPlugin {
    public:
        FfmpegPlugin() { }
        virtual void Release() { };
        virtual const char* Name() { return "ffmpeg IDecoder"; }
        virtual const char* Version() { return "0.1.0"; }
        virtual const char* Author() { return "clangen"; }
        virtual const char* Guid() { return "f993ec34-ab43-4c6a-9a0a-6462b4ae1a1c"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
} plugin;

class FfmpegDecoderFactory : public musik::core::sdk::IDecoderFactory {
    public:
        FfmpegDecoderFactory() {
#ifdef WIN32
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
            av_register_all();

            typeToCodecId = {
                { ".mp3", AV_CODEC_ID_MP3 },
                { "audio/mpeg", AV_CODEC_ID_MP3 },
                { ".ogg", AV_CODEC_ID_VORBIS },
                { "audio/ogg", AV_CODEC_ID_VORBIS },
                { ".opus", AV_CODEC_ID_OPUS },
                { ".flac", AV_CODEC_ID_FLAC },
                { "audio/flac", AV_CODEC_ID_FLAC },
                { ".aac", AV_CODEC_ID_AAC },
                { "audio/aac", AV_CODEC_ID_AAC },
                { ".mp4", AV_CODEC_ID_MPEG4 },
                { "audio/mp4", AV_CODEC_ID_MPEG4 },
                { ".m4a", AV_CODEC_ID_MPEG4 },
                { ".mpc", AV_CODEC_ID_MUSEPACK8 },
                { ".mp+", AV_CODEC_ID_MUSEPACK8 },
                { ".mpp", AV_CODEC_ID_MUSEPACK8 },
                { "audio/x-musepack", AV_CODEC_ID_MUSEPACK8 },
                { ".ape", AV_CODEC_ID_APE },
                { "audio/monkeys-audio", AV_CODEC_ID_APE },
                { ".wma", AV_CODEC_ID_WMAV2 },
                { "audio/x-ms-wma", AV_CODEC_ID_WMAV2 },
            };

            AVCodec* codec = av_codec_next(nullptr);
            while (codec != nullptr) {
                const AVCodecDescriptor* descriptor = avcodec_descriptor_get(codec->id);
                if (descriptor != nullptr && descriptor->type == AVMEDIA_TYPE_AUDIO) {
                    supported.insert(descriptor->id);
                }
                codec = av_codec_next(codec);
            }
        }

        ~FfmpegDecoderFactory() {
        }

        virtual void Release() {
        }

        virtual musik::core::sdk::IDecoder* CreateDecoder() {
            return new FfmpegDecoder();
        }

        virtual bool CanHandle(const char* type) const {
            std::string str(type);
            std::transform(str.begin(), str.end(), str.begin(), tolower);

            auto it = typeToCodecId.find(type);
            if (it != typeToCodecId.end()) {
                if (supported.find(it->second) != supported.end()) {
                    return true;
                }
            }

            return false;
        }

    private:
        std::map<std::string, AVCodecID> typeToCodecId;
        std::set<AVCodecID> supported;
} factory;

extern "C" DLLEXPORT musik::core::sdk::IPlugin* GetPlugin() {
    return &plugin;
}

extern "C" DLLEXPORT musik::core::sdk::IDecoderFactory* GetDecoderFactory() {
    return &factory;
}
