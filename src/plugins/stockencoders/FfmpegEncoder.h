//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/// @file FfmpegEncoder.h
/// @brief Offline (blocking) audio encoder built on FFmpeg.
/// @details Encodes PCM to a wide range of lossy/lossless audio formats through
/// libavcodec/libavformat. PCM is buffered in an audio FIFO, resampled to the
/// encoder's native format when needed, and written to an IDataStream. The
/// resulting file can be served to remote streaming clients.

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
/** @brief AVCodec type alias that accounts for const-ness in newer FFmpeg. */
using AVCodecCompat = const AVCodec;
#else
/** @brief AVCodec type alias that accounts for const-ness in older FFmpeg. */
using AVCodecCompat = AVCodec;
#endif

/** @brief Blocking audio encoder backed by FFmpeg.
 *  @details Initializes an output codec and muxer, feeds interleaved PCM
 *  through a FIFO and resampler, and writes encoded packets to the output
 *  stream. Used by the streaming server to transcode tracks to formats that
 *  are not available as real-time streaming encoders. */
class FfmpegEncoder : public musik::core::sdk::IBlockingEncoder {
    /** @brief Buffer type alias. */
    using IBuffer = musik::core::sdk::IBuffer;
    /** @brief Data stream type alias. */
    using IDataStream = musik::core::sdk::IDataStream;

    public:
        /** @brief Constructs an encoder for the given output format.
         *  @param format Muxer/output format name (e.g. "ogg", "mp3"). */
        FfmpegEncoder(const std::string& format);

        /** @brief Destroys the encoder and frees FFmpeg resources. */
        virtual void Release() override;
        /** @brief Opens the output codec and muxer.
         *  @param out The output data stream.
         *  @param rate Input sample rate in Hz.
         *  @param channels Input channel count.
         *  @param bitrate Target bitrate in bits per second.
         *  @return True if the encoder initialized successfully. */
        virtual bool Initialize(IDataStream* out, size_t rate, size_t channels, size_t bitrate) override;
        /** @brief Encodes a block of PCM.
         *  @param pcm The interleaved PCM buffer.
         *  @return True if the block was accepted. */
        virtual bool Encode(const IBuffer* pcm) override;
        /** @brief Flushes the encoder and writes the output trailer.
         *  @return True if the stream was finalized. */
        virtual void Finalize() override;

        /** @brief Returns the output data stream.
         *  @return The stream the encoded data is written to. */
        IDataStream* Stream() { return this->out; }

    private:
        /** @brief Releases all FFmpeg resources and allocations. */
        void Cleanup();
        /** @brief Opens and configures the output codec.
         *  @param rate Sample rate in Hz.
         *  @param channels Channel count.
         *  @param bitrate Target bitrate.
         *  @return True on success. */
        bool OpenOutputCodec(size_t rate, size_t channels, size_t bitrate);
        /** @brief Creates the muxer and I/O context.
         *  @return True on success. */
        bool OpenOutputContext();
        /** @brief Writes the container header.
         *  @return True on success. */
        bool WriteOutputHeader();
        /** @brief Writes the container trailer.
         *  @return True on success. */
        bool WriteOutputTrailer();
        /** @brief Queues PCM samples into the audio FIFO.
         *  @param pcm The PCM buffer.
         *  @return True on success. */
        bool WriteSamplesToFifo(const IBuffer* pcm);
        /** @brief Reads samples from the FIFO, encodes and writes them.
         *  @param drain True to drain all remaining samples.
         *  @return True on success. */
        bool ReadFromFifoAndWriteToOutput(bool drain);
        /** @brief Drains the resampler of remaining buffered samples. */
        void FlushResampler();
        /** @brief Allocates a frame with the given format.
         *  @param original Frame to reuse, or null.
         *  @param format The sample format.
         *  @param samplesPerChannel Samples per channel.
         *  @param sampleRate Sample rate in Hz.
         *  @return The (possibly reused) frame. */
        AVFrame* ReallocFrame(AVFrame* original, AVSampleFormat format, int samplesPerChannel, int sampleRate);
        /** @brief Sends a frame to the encoder and writes received packets.
         *  @param frame The frame to encode.
         *  @return 0 on success, or an error code. */
        int SendReceiveAndWriteFrame(AVFrame* frame);

        /** @brief Whether the encoder is usable. */
        bool isValid;
        /** @brief Output data stream. */
        IDataStream* out;
        /** @brief Scratch buffer size for I/O. */
        int readBufferSize;
        /** @brief FIFO buffering input PCM. */
        AVAudioFifo* outputFifo;
        /** @brief The selected output codec. */
        AVCodecCompat* outputCodec;
        /** @brief The output codec context. */
        AVCodecContext* outputContext;
        /** @brief The output muxer context. */
        AVFormatContext* outputFormatContext;
        /** @brief Custom I/O context writing to the stream. */
        AVIOContext* ioContext;
        /** @brief Buffer backing the custom I/O context. */
        void* ioContextOutputBuffer;
        /** @brief Frame fed to the encoder. */
        AVFrame* outputFrame;
        /** @brief Frame produced by the resampler. */
        AVFrame* resampledFrame;
        /** @brief Resampler converting input to the encoder format. */
        SwrContext* resampler;
        /** @brief Running presentation timestamp. */
        int64_t globalTimestamp;
        /** @brief Output format name. */
        std::string format;
        /** @brief Input channel count. */
        int inputChannelCount;
        /** @brief Input sample rate. */
        int inputSampleRate;
};
