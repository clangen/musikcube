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

#include <core/sdk/IDecoder.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

using namespace musik::core::sdk;

class OggDecoder : public IDecoder {
    public:
        OggDecoder();
        ~OggDecoder();

        virtual void Release() override;
        virtual double SetPosition(double second) override;
        virtual bool GetBuffer(IBuffer *buffer) override;
        virtual double GetDuration() override;
        virtual bool Open(musik::core::sdk::IDataStream *fileStream) override;
        virtual bool Exhausted() override { return this->exhausted; }

        /* libvorbis callbacks */
        static size_t OggRead(void *buffer, size_t nofParts, size_t partSize, void *datasource);
        static int OggSeek(void *datasource, ogg_int64_t offset, int whence);
        static long OggTell(void *datasource);
        static int OggClose(void *datasource);

    private:
        musik::core::sdk::IDataStream *fileStream;
        bool exhausted;
        OggVorbis_File oggFile;
        ov_callbacks oggCallbacks;
        double duration;
};
