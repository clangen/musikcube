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

#include <core/sdk/IDataStreamEncoder.h>
#include <core/sdk/DataBuffer.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avio.h>
    #include <libavformat/avformat.h>
    #include <libavutil/audio_fifo.h>
    #include <libswresample/swresample.h>
}

class FfmpegEncoder : public musik::core::sdk::IDataStreamEncoder {
    using IBuffer = musik::core::sdk::IBuffer;
    using IDataStream = musik::core::sdk::IDataStream;
    using IPreferences = musik::core::sdk::IPreferences;

    public:
        virtual void Release() override;
        virtual void Initialize(IDataStream* out, size_t rate, size_t channels, size_t bitrate) override;
        virtual bool Encode(const IBuffer* pcm) override;
        virtual void Finalize() override;
        virtual IPreferences* GetPreferences() override;

        IDataStream* Stream() { return this->out; }

    private:
        void Cleanup();
        bool OpenOutputCodec(size_t rate, size_t channels, size_t bitrate);
        bool OpenOutputContext();
        bool WriteOutputHeader();

        DataBuffer<char> resampledData;
        IDataStream* out;
        IPreferences* prefs;
        int readBufferSize;
        bool isValid{ false };
        AVAudioFifo* outputFifo;
        AVCodec* outputCodec;
        AVCodecContext* outputContext;
        AVFormatContext* outputFormatContext;
        AVIOContext* ioContext;
        void* ioContextOutputBuffer;
        AVFrame* outputFrame;
        SwrContext* resampler;
        int64_t globalTimestamp;
};