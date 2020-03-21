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

#pragma once

#include "Context.h"
#include <core/sdk/constants.h>
#include <core/sdk/IDataStream.h>
#include <core/sdk/IDecoder.h>
#include <core/sdk/IStreamingEncoder.h>
#include <string>

class Transcoder {
    public:
        using IDataStream = musik::core::sdk::IDataStream;
        using IEncoder = musik::core::sdk::IEncoder;
        using IStreamingEncoder = musik::core::sdk::IStreamingEncoder;

        static void RemoveTempTranscodeFiles(Context& context);

        static void PruneTranscodeCache(Context& context);

        static IDataStream* Transcode(
            Context& context,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        static IDataStream* TranscodeAndWait(
            Context& context,
            IEncoder* encoder,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

    private:
        static IDataStream* TranscodeOnDemand(
            Context& context,
            IStreamingEncoder* encoder,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        Transcoder() { }
        ~Transcoder() { }
};