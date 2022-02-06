//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#ifdef WIN32
#define ENABLE_FFMPEG
#endif

#ifdef ENABLE_FFMPEG

#include <musikcore/sdk/IBlockingEncoder.h>
#include <musikcore/sdk/DataBuffer.h>
#include <string>

extern "C" {
    #pragma warning(push, 0)
    #include <libavcodec/avcodec.h>
    #include <libavcodec/version.h>
    #include <libavformat/avio.h>
    #include <libavformat/avformat.h>
    #include <libavutil/audio_fifo.h>
    #include <libswresample/swresample.h>
    #pragma warning(pop)
}

#if LIBAVCODEC_VERSION_MAJOR >= 59
using resolved_avcodec_type = const AVCodec;
#else
using resolved_avcodec_type = AVCodec;
#endif

class FfmpegEncoder : public musik::core::sdk::IBlockingEncoder {
    using IBuffer = musik::core::sdk::IBuffer;
    using IDataStream = musik::core::sdk::IDataStream;

    public:
        FfmpegEncoder(const std::string& format);

        virtual void Release() override;
        virtual bool Initialize(IDataStream* out, size_t rate, size_t channels, size_t bitrate) override;
        virtual bool Encode(const IBuffer* pcm) override;
        virtual void Finalize() override;

        IDataStream* Stream() { return this->out; }

    private:
        void Cleanup();
        bool OpenOutputCodec(size_t rate, size_t channels, size_t bitrate);
        bool OpenOutputContext();
        bool WriteOutputHeader();
        bool WriteOutputTrailer();
        bool WriteSamplesToFifo(const IBuffer* pcm);
        bool ReadFromFifoAndWriteToOutput(bool drain);
        void FlushResampler();
        AVFrame* ReallocFrame(AVFrame* original, AVSampleFormat format, int samplesPerChannel, int sampleRate);
        int SendReceiveAndWriteFrame(AVFrame* frame);

        bool isValid;
        IDataStream* out;
        int readBufferSize;
        AVAudioFifo* outputFifo;
        resolved_avcodec_type* outputCodec;
        AVCodecContext* outputContext;
        AVFormatContext* outputFormatContext;
        AVIOContext* ioContext;
        void* ioContextOutputBuffer;
        AVFrame* outputFrame;
        AVFrame* resampledFrame;
        SwrContext* resampler;
        int64_t globalTimestamp;
        std::string format;
        int inputChannelCount;
        int inputSampleRate;
};

#endif // ENABLE_FFMPEG
