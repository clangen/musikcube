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

#include <core/sdk/IAudioStreamEncoder.h>
#include <core/sdk/DataBuffer.h>
#include <vorbis/vorbisenc.h>

/* fre:ac/BoCA has an excellent example of vorbis encoder usage, a lot of code
was adapted (stolen) from here: https://github.com/enzo1982/BoCA/blob/master/components/encoder/vorbis/vorbis.cpp */

class OggEncoder : public musik::core::sdk::IAudioStreamEncoder {
    using IBuffer = musik::core::sdk::IBuffer;

    public:
        virtual void Release() override;
        virtual void Initialize(size_t rate, size_t channels, size_t bitrate) override;
        virtual int Encode(const IBuffer* pcm, char** data) override;
        virtual int Flush(char** data) override;
        virtual void Finalize(const char* uri) override;
        virtual musik::core::sdk::IPreferences* GetPreferences() override;

    private:
        int WritePackets(bool flush);

        DataBuffer<char> encodedData;
        musik::core::sdk::IPreferences* prefs;
        long bitrate;
        bool headerWritten;
        ogg_stream_state os;
        ogg_page og;
        ogg_packet op;
        vorbis_info vi;
        vorbis_comment vc;
        vorbis_dsp_state vd;
        vorbis_block vb;
};