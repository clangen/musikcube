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

#include "FfmpegDecoder.h"
#include <musikcore/sdk/IDebug.h>
#include <algorithm>
#include <string>
#include <unordered_set>

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#define DEFAULT_FRAME_SIZE 4096
#define BUFFER_SIZE 4096
#define PROBE_SIZE 32768

#if LIBAVUTIL_VERSION_MAJOR >= 59
#define USE_FFMPEG7_CHANNEL_LAYOUT
#endif

using namespace musik::core::sdk;

static const char* TAG = "ffmpegdecoder";
static IDebug* debug = nullptr;

static std::unordered_set<AVCodecID> ignoreInvalidPacketCodecs = { AV_CODEC_ID_APE };

#define RESOLVE_SAMPLE_RATE() this->preferredSampleRate > 0 ? this->preferredSampleRate : this->rate

extern "C" DLLEXPORT void SetDebug(IDebug* debug) {
    ::debug = debug;
}

static std::string getAvError(int errnum) {
    char buffer[AV_ERROR_MAX_STRING_SIZE];
    buffer[0] = '\0';
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, errnum);
    return std::string(buffer);
}

static void logAvError(const std::string& method, int errnum) {
#ifndef ENABLE_LOG_AVERROR_EOF
    if (errnum == AVERROR_EOF) {
        return; /* these are generally legit; no need to pollute the log */
    }
#endif
    if (errnum != 0) {
        std::string err = method + "() failed: " + getAvError(errnum);
        ::debug->Warning(TAG, err.c_str());
    }
}

static void logError(const std::string& message) {
    ::debug->Warning(TAG, message.c_str());
}

#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
static AVChannelLayout resolveChannelLayout(size_t channelCount) {
    AVChannelLayout result;
    memset(&result, 0, sizeof(result));

    result.nb_channels = channelCount;
    switch (channelCount) {
        case 1: result.u.mask = AV_CH_LAYOUT_MONO; break;
        case 2: result.u.mask =  AV_CH_LAYOUT_STEREO; break;
        case 3: result.u.mask =  AV_CH_LAYOUT_2POINT1; break;
        case 4: result.u.mask =  AV_CH_LAYOUT_3POINT1; break;
        case 5: result.u.mask =  AV_CH_LAYOUT_4POINT1; break;
        case 6: result.u.mask =  AV_CH_LAYOUT_5POINT1; break;
        default: result.u.mask =  AV_CH_LAYOUT_STEREO_DOWNMIX; break;
    }

    return result;
}
#endif

static int readCallback(void* opaque, uint8_t* buffer, int bufferSize) {
    FfmpegDecoder* decoder = static_cast<FfmpegDecoder*>(opaque);
    if (decoder && decoder->Stream()) {
        auto count = decoder->Stream()->Read(buffer, (PositionType) bufferSize);
        if (count > 0) {
            return count;
        }
    }
    return AVERROR_EOF;
}

#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
    static int writeCallback(void* opaque, const uint8_t* buffer, int bufferSize) {
#else
    static int writeCallback(void* opaque, uint8_t* buffer, int bufferSize) {
#endif
    return 0;
}

static int64_t seekCallback(void* opaque, int64_t offset, int whence) {
    FfmpegDecoder* decoder = static_cast<FfmpegDecoder*>(opaque);
    if (decoder && decoder->Stream()) {
        IDataStream* stream = decoder->Stream();
        switch (whence) {
            case AVSEEK_SIZE:
                return stream->Length();
            case SEEK_SET: {
                if (offset >= stream->Length()) {
                    debug->Error(TAG, "SEEK_SET requested offset beyond EOF");
                    return AVERROR(EINVAL);
                }
                if (!stream->SetPosition((PositionType) offset)) {
                    debug->Error(TAG, "SEEK_SET failed");
                }
                break;
            }
            case SEEK_CUR: {
                if (stream->Position() + offset >= stream->Length()) {
                    debug->Error(TAG, "SEEK_CUR requested offset beyond EOF");
                    return AVERROR(EINVAL);
                }
                if (!stream->SetPosition(stream->Position() + (PositionType) offset)) {
                    debug->Error(TAG, "SEEK_CUR failed");
                }
                break;
            }
            case SEEK_END:
                if (!stream->SetPosition(stream->Length() - 1)) {
                    debug->Error(TAG, "SEEK_END failed");
                }
                break;
            default:
                debug->Error(TAG, "unknown seek type!");
                break;
        }

        return stream->Position();
    }
    return 0;
}

FfmpegDecoder::FfmpegDecoder() {
    this->stream = nullptr;
    this->streamId = -1;
    this->duration = -1.0f;
    this->ioContext = nullptr;
    this->formatContext = nullptr;
    this->codecContext = nullptr;
    this->decodedFrame = nullptr;
    this->resampledFrame = nullptr;
    this->resampler = nullptr;
    this->outputFifo = nullptr;
    this->preferredSampleRate = -1;
    this->rate = 0;
    this->preferredFrameSize = 0;
    this->channels = 0;
}

FfmpegDecoder::~FfmpegDecoder() {
    this->Reset();

    if (this->decodedFrame) {
        av_frame_free(&this->decodedFrame);
        this->decodedFrame = nullptr;
    }

    if (this->resampledFrame) {
        av_frame_free(&this->resampledFrame);
        this->resampledFrame = nullptr;
    }

    if (this->resampler) {
        swr_free(&this->resampler);
        this->resampler = nullptr;
    }
}

void FfmpegDecoder::Release() {
    delete this;
}

double FfmpegDecoder::SetPosition(double seconds) {
    if (this->ioContext && this->formatContext && this->codecContext) {
        AVStream* stream = this->formatContext->streams[this->streamId];
        AVRational timeBase = stream->time_base;
        int64_t seekTime = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;
        seekTime += av_rescale((int64_t) seconds, timeBase.den, timeBase.num);
        if (av_seek_frame(this->formatContext, this->streamId, seekTime, AVSEEK_FLAG_ANY) >= 0) {
            return seconds;
        }
    }
    return -1.0f;
}

bool FfmpegDecoder::GetBuffer(IBuffer *buffer) {
    if (this->ioContext) {
        buffer->SetSampleRate((long) RESOLVE_SAMPLE_RATE());
        buffer->SetChannels((long) this->channels);
        buffer->SetSamples(0);

        if (!this->eof) {
            if (!this->resampler && !this->InitializeResampler()) {
                this->exhausted = true;
                logError("unable to initialize resampler. marking as done.");
                return false;
            }

            if (av_audio_fifo_size(this->outputFifo) < this->preferredFrameSize) {
                if (!this->RefillFifoQueue()) {
                    this->FlushAndFinalizeDecoder();
                    this->DrainResamplerToFifoQueue();
                    this->eof = true;
                }
            }
        }
        if (this->ReadFromFifoAndWriteToBuffer(buffer)) {
             return true;
        }
    }

    ::debug->Info(TAG, "finished decoding.");
    this->exhausted = true;
    return false;
}

double FfmpegDecoder::GetDuration() {
    return this->duration;
}

void FfmpegDecoder::Reset() {
    if (this->ioContext) {
        av_free(this->ioContext->buffer);
        av_free(this->ioContext);
        this->ioContext = nullptr;
    }
    if (this->codecContext) {
        // avcodec_flush_buffers(this->codecContext);
        auto stream = this->formatContext->streams[this->streamId];
        if (stream != nullptr) {
            avcodec_close(this->codecContext);
        }

        this->codecContext = nullptr;
    }
    if (this->formatContext) {
        avformat_close_input(&this->formatContext);
        avformat_free_context(this->formatContext);
        this->formatContext = nullptr;
    }
    if (this->outputFifo) {
        av_audio_fifo_free(this->outputFifo);
        this->outputFifo = nullptr;
    }
    if (this->resampler) {
        swr_free(&this->resampler);
        this->resampler = nullptr;
    }
    this->streamId = -1;
}

bool FfmpegDecoder::InitializeResampler() {
    if (this->resampler) {
        swr_free(&this->resampler);
        this->resampler = nullptr;
    }

    int outSampleRate = (int) RESOLVE_SAMPLE_RATE();

#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
    int error = swr_alloc_set_opts2(
        &this->resampler,
        &this->codecContext->ch_layout,
#else
    this->resampler = swr_alloc_set_opts(
        this->resampler,
        this->codecContext->channel_layout,
#endif
        AV_SAMPLE_FMT_FLT,
        outSampleRate,
#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
        &this->codecContext->ch_layout,
#else
        this->codecContext->channel_layout,
#endif
        this->codecContext->sample_fmt,
        this->codecContext->sample_rate,
        0,
        nullptr);

#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
    if (error) {
        logAvError("swr_alloc_set_opts2", error);
        return false;
    }
#else
    int error = 0;
#endif

    if ((error = swr_init(this->resampler)) != 0) {
        logAvError("swr_init", error);
        return false;
    }

    return true;
}

bool FfmpegDecoder::Open(musik::core::sdk::IDataStream *stream) {
    if (stream->Seekable() && this->ioContext == nullptr) {
        ::debug->Info(TAG, "parsing data stream...");

        this->stream = stream;

        const int ioContextBufferSize = AV_INPUT_BUFFER_PADDING_SIZE + BUFFER_SIZE;
        unsigned char* ioContextBuffer = (unsigned char*) av_malloc(ioContextBufferSize);

        this->ioContext = avio_alloc_context(
            ioContextBuffer,
            ioContextBufferSize,
            0,
            this,
            readCallback,
            writeCallback,
            seekCallback);

        if (this->ioContext != nullptr) {
            this->streamId = -1;
            this->formatContext = avformat_alloc_context();
            this->formatContext->pb = this->ioContext;
            this->formatContext->flags = AVFMT_FLAG_CUSTOM_IO;

            unsigned char probe[PROBE_SIZE];
            memset(probe, 0, PROBE_SIZE);
            int count = stream->Read(probe, PROBE_SIZE - AVPROBE_PADDING_SIZE);
            stream->SetPosition(0);

            AVProbeData probeData = { 0 };
            probeData.buf = probe;
            probeData.buf_size = count;
            probeData.filename = "";

            this->formatContext->iformat = av_probe_input_format(&probeData, 1);

            if (this->formatContext->iformat) {
                if (avformat_open_input(&this->formatContext, "", nullptr, nullptr) == 0) {
                    AVCodecCompat* codec = nullptr;
                    if (avformat_find_stream_info(this->formatContext, nullptr) >= 0) {
                        this->streamId = av_find_best_stream(
                            this->formatContext,
                            AVMEDIA_TYPE_AUDIO,
                            -1,
                            -1,
                            &codec,
                            0);
                    }

                    if (this->streamId != -1 && codec != nullptr) {
                        ::debug->Info(TAG, "found audio stream!");
                        this->codecContext = avcodec_alloc_context3(codec);
                        if (codecContext) {
                            this->codecContext->request_sample_fmt = AV_SAMPLE_FMT_FLT;
                            if (codec) {
                                int error = avcodec_parameters_to_context(
                                    this->codecContext,
                                    formatContext->streams[this->streamId]->codecpar);
                                if (error < 0) {
                                    logAvError("avcodec_parameters_to_context", error);
                                    goto reset_and_fail;
                                }

                                error = avcodec_open2(codecContext, codec, nullptr);
                                if (error < 0) {
                                    logAvError("avcodec_open2", error);
                                    goto reset_and_fail;
                                }

                                std::string codecName =
                                    std::string("resolved codec: ") +
                                    std::string(codec->long_name);
                                ::debug->Info(TAG, codecName.c_str());
                            }
                            else {
                                ::debug->Error(TAG, "couldn't find a codec.");
                                goto reset_and_fail;
                            }

#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
                            if (this->codecContext->ch_layout.nb_channels == 0) {
                                this->codecContext->ch_layout =
                                    resolveChannelLayout(this->codecContext->ch_layout.nb_channels);
#else
                            if (this->codecContext->channel_layout == 0) {
                                this->codecContext->channel_layout =
                                    av_get_default_channel_layout(this->codecContext->channels);
#endif
                            }

                            this->preferredFrameSize = this->codecContext->frame_size
                                ? this->codecContext->frame_size
                                : DEFAULT_FRAME_SIZE;

                            this->disableInvalidPacketDetection =
                                ignoreInvalidPacketCodecs.find(this->codecContext->codec_id) !=
                                ignoreInvalidPacketCodecs.end();
                        }

                        auto stream = this->formatContext->streams[this->streamId];
                        this->rate = stream->codecpar->sample_rate;
#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
                        this->channels = stream->codecpar->ch_layout.nb_channels;
#else
                        this->channels = stream->codecpar->channels;
#endif
                        this->duration = (double) this->formatContext->duration / (double) AV_TIME_BASE;
                        this->outputFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, channels, 1);

                        if (!this->outputFifo) {
                            logError("av_audio_fifo_alloc");
                            goto reset_and_fail;
                        }

                        return true;
                    }
                    else {
                        ::debug->Error(TAG, "audio stream not found in input data.");
                    }
                }
            }
        }
    }

reset_and_fail:
    ::debug->Error(TAG, "failed to find compatible audio stream");
    this->Reset();
    return false;
}

bool FfmpegDecoder::Exhausted() {
    return this->exhausted;
}

bool FfmpegDecoder::ReadSendAndReceivePacket(AVPacket* packet) {
    bool decodedAtLeastOneFrame = false;
    int error = avcodec_send_packet(this->codecContext, packet);
    while (error >= 0) {
        this->decodedFrame = this->AllocFrame(
            this->decodedFrame,
            this->codecContext->sample_fmt,
            this->codecContext->sample_rate);

        error = avcodec_receive_frame(this->codecContext, this->decodedFrame);
        if (error >= 0) {
            this->resampledFrame = this->AllocFrame(
                this->resampledFrame,
                AV_SAMPLE_FMT_FLT,
                RESOLVE_SAMPLE_RATE(),
                this->decodedFrame->nb_samples);

            error = swr_convert_frame(
                this->resampler,
                this->resampledFrame,
                this->decodedFrame);

            if (error < 0) {
                logAvError("swr_convert_frame", error);
                this->InitializeResampler();
                error = swr_convert_frame(
                    this->resampler,
                    this->resampledFrame,
                    this->decodedFrame);
            }

            if (error >= 0) {
                error = av_audio_fifo_write(
                    this->outputFifo,
                    (void**) this->resampledFrame->extended_data,
                    this->resampledFrame->nb_samples);

                if (error < 0) {
                    logAvError("av_audio_fifo_write", error);
                    return false;
                }

                /* there may still be pending data in the resampler, go ahead
                and write to the fifo right now so it doesn't back up */
                this->DrainResamplerToFifoQueue();

                decodedAtLeastOneFrame = true;
            }
        }
    }
    return decodedAtLeastOneFrame;
}

bool FfmpegDecoder::DrainResamplerToFifoQueue() {
    if (!this->resampler) {
        return false;
    }

    const int64_t targetRate =
        RESOLVE_SAMPLE_RATE() > this->codecContext->sample_rate
            ? RESOLVE_SAMPLE_RATE()
            : this->codecContext->sample_rate;

    int64_t bufferedFrames = swr_get_delay(this->resampler, targetRate);

    while (bufferedFrames > 0) {
        this->resampledFrame = this->AllocFrame(
            this->resampledFrame,
            this->codecContext->sample_fmt,
            this->codecContext->sample_rate);

        int converted = swr_convert(
            this->resampler,
            this->resampledFrame->extended_data,
            this->resampledFrame->nb_samples,
            nullptr,
            0);

        if (converted > 0) {
            int error = av_audio_fifo_write(
                this->outputFifo,
                (void**) this->resampledFrame->extended_data,
                converted);

            if (error < 0) {
                logAvError("av_audio_fifo_write", error);
                return false;
            }

            bufferedFrames -= converted;
        }
        else {
            break;
        }
    }

    return true;
}

bool FfmpegDecoder::RefillFifoQueue() {
    bool sentAtLeastOnePacket = false;
    bool readFailed = false;
    int fifoSize = av_audio_fifo_size(this->outputFifo);
    while (!readFailed && fifoSize < this->preferredFrameSize) {
        AVPacket packet;
        memset(&packet, 0, sizeof(AVPacket));
        packet.pts = AV_NOPTS_VALUE;
        packet.dts = AV_NOPTS_VALUE;
        packet.pos = -1;
        int error = av_read_frame(this->formatContext, &packet);

        if (error >= 0) {
            /* note that sometimes decoders seem to return packets that are
            invalid. this can be observed when playing wav files that have
            album art metadata, but may happen in other cases. if we detect
            an invalid packet, simply discard it and get the next one. to make
            things worse, some decoders (e.g. APE) always return packets in this
            state. we're missing something, somewhere during codec init probably,
            but after hours of messing around I can't figure it out. if you
            are reading this comment and have any ideas, please let me know. */
            if (packet.pos == -1 &&
                packet.duration <= 1 &&
                !this->disableInvalidPacketDetection)
            {
                logError("invalid packet detected, discarding.");
            }
            else {
                sentAtLeastOnePacket = this->ReadSendAndReceivePacket(&packet);
            }
        }
        else {
            logAvError("av_read_frame", error);
            readFailed = true;
        }

        av_packet_unref(&packet);
        fifoSize = av_audio_fifo_size(this->outputFifo);
    }
    return sentAtLeastOnePacket;
}

bool FfmpegDecoder::ReadFromFifoAndWriteToBuffer(IBuffer* buffer) {
    int error = 0;
    int fifoSize = av_audio_fifo_size(this->outputFifo);

    if (this->eof && fifoSize == 0) {
        return false;
    }

    if (
        fifoSize >= this->preferredFrameSize ||
        (this->eof && fifoSize > 0)
    ) {
        const int expectedFrameSize = FFMIN(fifoSize, this->preferredFrameSize);
        buffer->SetSamples(expectedFrameSize * this->channels);
        void* outData = (void*)buffer->BufferPointer();
        int actualFrameSize = av_audio_fifo_read(this->outputFifo, &outData, expectedFrameSize);

        if (actualFrameSize > expectedFrameSize) {
            logError("av_audio_fifo_read read the incorrect number of samples");
            return false;
        }
        else if (actualFrameSize != expectedFrameSize) {
            buffer->SetSamples(actualFrameSize * this->channels);
        }
    }

    return true;
}

void FfmpegDecoder::FlushAndFinalizeDecoder() {
    /* reading from a packet with a null `data` pointer is a "flush" operation,
    and the decoder will stop accepting new data */
    this->ReadSendAndReceivePacket(nullptr);
}

AVFrame* FfmpegDecoder::AllocFrame(AVFrame* original, AVSampleFormat format, int sampleRate, int frameSize) {
    bool frameSizeChanged = original && frameSize > 0 && frameSize != original->nb_samples;
    if (!original || frameSizeChanged) {
        if (original || frameSizeChanged) {
            av_frame_free(&original);
        }
#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
        const AVChannelLayout channelLayout = this->codecContext->ch_layout.nb_channels == 0
            ? resolveChannelLayout(this->codecContext->ch_layout.nb_channels)
            : this->codecContext->ch_layout;
        original = av_frame_alloc();
        original->ch_layout = channelLayout;
#else
        const int channelLayout = this->codecContext->channel_layout == 0
            ? av_get_default_channel_layout(this->codecContext->channels)
            : this->codecContext->channel_layout;
        original = av_frame_alloc();
        original->channel_layout = channelLayout;
#endif
        original->format = format;
        original->sample_rate = sampleRate;
        if (frameSizeChanged) {
            original->nb_samples = frameSize;
            av_frame_get_buffer(original, 0);
        }
    }
    return original;
}
