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

/** @file FfmpegDecoder.h
 *  @brief FFmpeg-based audio decoder plugin.
 *  @details Decodes virtually any audio format supported by the linked
 *  FFmpeg/Libav libraries (MP3, FLAC, AAC, Ogg Vorbis, Opus, WMA, AC-3 and
 *  many more). Reads from an arbitrary IDataStream through a custom AVIO
 *  context, resamples to the preferred sample rate and emits float/interleaved
 *  PCM via an audio FIFO.
 */

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IDataStream.h>

extern "C" {
    #pragma warning(push, 0)
    #include <libavformat/avio.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/version.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/audio_fifo.h>
    #include <libswresample/swresample.h>
    #pragma warning(pop)
}

#include <stddef.h>

#if LIBAVCODEC_VERSION_MAJOR >= 59
/** @brief Const-correct AVCodec alias for FFmpeg 5+ (Libavcodec 59). */
using AVCodecCompat = const AVCodec;
#else
/** @brief Non-const AVCodec alias for older FFmpeg versions. */
using AVCodecCompat = AVCodec;
#endif

using namespace musik::core::sdk;

/** @brief FFmpeg-backed decoder implementing the musikcube decoder SDK.
 *  @details Supports seeking, duration queries and resampling. The input is
 *  pulled from an IDataStream via a custom AVIO context so it can decode from
 *  any plugin data stream. Output is produced through an internal AVAudioFifo
 *  and delivered as interleaved float PCM.
 */
class FfmpegDecoder: public musik::core::sdk::IDecoder {
    public:
        FfmpegDecoder();
        ~FfmpegDecoder();

        /** @brief Destroys the decoder and releases FFmpeg resources. */
        void Release() override;
        /** @brief Seeks to a position in the stream.
         *  @param seconds Target position in seconds.
         *  @return The actual position reached, in seconds. */
        double SetPosition(double seconds) override;
        /** @brief Fills a buffer with the next decoded PCM data.
         *  @param buffer The buffer to fill with decoded samples.
         *  @return True if data was written, false on error. */
        bool GetBuffer(IBuffer *buffer) override;
        /** @brief Returns the total duration of the stream.
         *  @return Duration in seconds. */
        double GetDuration() override;
        /** @brief Opens the decoder against a data stream.
         *  @param stream The input data stream to decode.
         *  @return True if the stream opened successfully. */
        bool Open(musik::core::sdk::IDataStream *stream) override;
        /** @brief Returns whether the decoder has reached the end of input.
         *  @return True when no more audio can be decoded. */
        bool Exhausted() override;
        /** @brief Sets the preferred output sample rate.
         *  @param rate The sample rate in Hz, or -1 to keep the source rate. */
        void SetPreferredSampleRate(int rate) override { this->preferredSampleRate = rate; }

        /** @brief Returns the underlying input stream.
         *  @return The IDataStream being decoded. */
        IDataStream* Stream() { return this->stream; }

    private:
        /** @brief Resets the decoder state so it can be reopened. */
        void Reset();
        /** @brief Allocates a frame in the given format.
         *  @param original Template frame used to size the new frame.
         *  @param format The target sample format.
         *  @param sampleRate The target sample rate.
         *  @param frameSize Optional frame size in samples.
         *  @return A newly allocated AVFrame, or null on failure. */
        AVFrame* AllocFrame(AVFrame* original, AVSampleFormat format, int sampleRate, int frameSize = -1);
        /** @brief Decodes packets and feeds decoded frames into the FIFO.
         *  @return True on success, false on error. */
        bool RefillFifoQueue();
        /** @brief Drains the resampler output into the FIFO.
         *  @return True on success, false on error. */
        bool DrainResamplerToFifoQueue();
        /** @brief Reads PCM data from the FIFO into the caller's buffer.
         *  @param buffer The buffer to fill.
         *  @return True if data was read. */
        bool ReadFromFifoAndWriteToBuffer(IBuffer* buffer);
        /** @brief Creates the resampler used to reach the preferred rate.
         *  @return True if the resampler was initialized. */
        bool InitializeResampler();
        /** @brief Reads a packet from the stream, sends it to the decoder and
         *  receives any decoded frames.
         *  @param packet The packet to process.
         *  @return True on success, false on error. */
        bool ReadSendAndReceivePacket(AVPacket* packet);
        /** @brief Flushes the decoder and finalizes internal state. */
        void FlushAndFinalizeDecoder();

        /** @brief The input data stream being decoded. */
        musik::core::sdk::IDataStream* stream;
        /** @brief Custom AVIO context bridging FFmpeg to the IDataStream. */
        AVIOContext* ioContext;
        /** @brief FIFO holding decoded PCM samples awaiting delivery. */
        AVAudioFifo* outputFifo;
        /** @brief Demuxer context for the input file. */
        AVFormatContext* formatContext;
        /** @brief Codec context for the audio stream. */
        AVCodecContext* codecContext;
        /** @brief Frame holding the last decoded audio. */
        AVFrame* decodedFrame;
        /** @brief Frame holding the resampled audio. */
        AVFrame* resampledFrame;
        /** @brief Resampler converting to the preferred sample rate/format. */
        SwrContext* resampler;
        /** @brief Preferred output sample rate, or -1 for source rate. */
        int preferredSampleRate { -1 };
        /** @brief Disables detection of malformed packets. */
        bool disableInvalidPacketDetection { false };
        /** @brief Output sample rate and channel count. */
        int rate, channels;
        /** @brief Index of the audio stream within the container. */
        int streamId;
        /** @brief Preferred output frame size in samples. */
        int preferredFrameSize;
        /** @brief Total duration of the stream in seconds. */
        double duration;
        /** @brief True when no more audio remains. */
        bool exhausted{ false };
        /** @brief True when end-of-file has been reached. */
        bool eof{ false };
};
