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

#include <core/sdk/IDataStream.h>
#include <core/sdk/IStreamingEncoder.h>
#include <core/sdk/DataBuffer.h>
#include "Context.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <stdio.h>

class TranscodingAudioDataStream : public musik::core::sdk::IDataStream {
    public:
        using PositionType = musik::core::sdk::PositionType;
        using OpenFlags = musik::core::sdk::OpenFlags;

        TranscodingAudioDataStream(
            Context& context,
            musik::core::sdk::IStreamingEncoder* encoder,
            const std::string& uri,
            size_t bitrate,
            const std::string& format);

        TranscodingAudioDataStream(
            Context& context,
            musik::core::sdk::IStreamingEncoder* encoder,
            const std::string& uri,
            const std::string& tempFilename,
            const std::string& finalFilename,
            size_t bitrate,
            const std::string& format);

        virtual ~TranscodingAudioDataStream();

        virtual bool Open(const char *uri, OpenFlags flags) override;
        virtual bool Close() override;
        virtual void Interrupt() override;
        virtual void Release() override;
        virtual bool Readable() override { return true; }
        virtual bool Writable() override { return false; };
        virtual PositionType Read(void *buffer, PositionType readBytes) override;
        virtual PositionType Write(void *buffer, PositionType writeBytes) override { return 0; }
        virtual bool SetPosition(PositionType position) override;
        virtual PositionType Position() override;
        virtual bool Seekable() override;
        virtual bool Eof() override;
        virtual long Length() override;
        virtual const char* Type() override;
        virtual const char* Uri() override;
        virtual bool CanPrefetch() override;

    private:
        void Dispose();

        Context& context;
        musik::core::sdk::IDataStream* input;
        musik::core::sdk::IDecoder* decoder;
        musik::core::sdk::IBuffer* pcmBuffer;
        musik::core::sdk::IStreamingEncoder* encoder;
        DataBuffer<char> spillover;
        size_t bitrate;
        bool eof;
        std::mutex mutex;
        PositionType length, position;
        FILE* outFile;
        std::string tempFilename, finalFilename;
        std::string format;
        bool interrupted;
        long detachTolerance;
};