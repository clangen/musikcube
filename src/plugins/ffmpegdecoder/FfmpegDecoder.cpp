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

#include "FfmpegDecoder.h"
#include <core/sdk/IDebug.h>
#include <algorithm>
#include <string>

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#define BUFFER_SIZE 4096
#define PROBE_SIZE 32768

using namespace musik::core::sdk;

static const char* TAG = "ffmpegdecoder";
static IDebug* debug = nullptr;

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
    if (errnum != 0) {
        std::string err = method + "() failed: " + getAvError(errnum);
        ::debug->Warning(TAG, err.c_str());
    }
}

static int readCallback(void* opaque, uint8_t* buffer, int bufferSize) {
    FfmpegDecoder* decoder = static_cast<FfmpegDecoder*>(opaque);
    if (decoder && decoder->Stream()) {
        auto count = decoder->Stream()->Read(buffer, (PositionType) bufferSize);
        return (count == bufferSize) ? count : AVERROR_EOF;
    }
    return 0;
}

static int writeCallback(void* opaque, uint8_t* buffer, int bufferSize) {
    return 0;
}

static int64_t seekCallback(void* opaque, int64_t offset, int whence) {
    FfmpegDecoder* decoder = static_cast<FfmpegDecoder*>(opaque);
    if (decoder && decoder->Stream()) {
        IDataStream* stream = decoder->Stream();
        switch (whence) {
            case AVSEEK_SIZE:
                return stream->Length();
                break;
            case SEEK_SET:
                stream->SetPosition((PositionType) offset);
                break;
            case SEEK_CUR:
                stream->SetPosition(stream->Position() + (PositionType) offset);
                break;
            case SEEK_END:
                stream->SetPosition(stream->Length() - 1);
                break;
            default:
                debug->Error(TAG, "unknown seek type!");
                break;
        }

        if (stream->Position() >= stream->Length()) {
            return -1;
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
    this->resampler = nullptr;
    this->bufferSize = AV_INPUT_BUFFER_PADDING_SIZE + BUFFER_SIZE;
    this->buffer = new unsigned char[this->bufferSize];
    this->decodedFrame = av_frame_alloc();
    av_init_packet(&this->packet);
}

FfmpegDecoder::~FfmpegDecoder() {
    this->Reset();

    delete[] this->buffer;
    this->buffer = nullptr;
    av_free_packet(&this->packet);
    av_frame_free(&this->decodedFrame);
    this->decodedFrame = nullptr;

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
        AVRational baseQ = { 1, AV_TIME_BASE };
        int64_t pts = av_rescale_q((int64_t)(seconds * AV_TIME_BASE), baseQ, stream->time_base);
        avcodec_flush_buffers(this->codecContext);
        if (av_seek_frame(this->formatContext, this->streamId, pts, 0) >= 0) {
            return seconds;
        }
    }
    return -1.0f;
}

bool FfmpegDecoder::GetBuffer(IBuffer *buffer) {
    if (this->ioContext) {
        buffer->SetSampleRate((long) this->rate);
        buffer->SetChannels((long) this->channels);
        buffer->SetSamples(0);

        int errnum = av_read_frame(this->formatContext, &this->packet);

        if (!errnum) {
            int frameDecoded = 0;

            avcodec_decode_audio4(
                this->codecContext,
                this->decodedFrame,
                &frameDecoded,
                &this->packet);

            av_free_packet(&this->packet);

            if (frameDecoded) {
                int samples = this->decodedFrame->nb_samples;
                int channels = this->codecContext->channels;
                auto inFormat = this->codecContext->sample_fmt;
                auto inLayout = this->codecContext->channel_layout;

                if (inLayout == 0) {
                    /* in some cases it seems the input channel layout cannot be detected.
                    for this case, we configure it manually so the resampler is happy. */
                    switch (channels) {
                        case 1: inLayout = AV_CH_LAYOUT_MONO; break;
                        case 2: inLayout = AV_CH_LAYOUT_STEREO; break;
                        case 3: inLayout = AV_CH_LAYOUT_2POINT1; break;
                        case 4: inLayout = AV_CH_LAYOUT_3POINT1; break;
                        case 5: inLayout = AV_CH_LAYOUT_4POINT1; break;
                        case 6: inLayout = AV_CH_LAYOUT_5POINT1; break;
                        default: inLayout = AV_CH_LAYOUT_STEREO_DOWNMIX; break;
                    }
                }

                int decodedSize = av_samples_get_buffer_size(
                    nullptr, channels, samples, inFormat, 1);

                if (decodedSize > 0) {
                    /* preferred buffer size based on input data */
                    buffer->SetSamples(samples * channels);

                    if (!this->resampler) {
                        this->resampler = swr_alloc();

                        swr_alloc_set_opts(
                            this->resampler,
                            inLayout,
                            AV_SAMPLE_FMT_FLT,
                            this->rate,
                            inLayout,
                            inFormat,
                            this->rate,
                            0,
                            nullptr);

                        if ((errnum = swr_init(this->resampler)) != 0) {
                            logAvError("swr_init", errnum);
                        }
                    }

                    uint8_t* outData = (uint8_t*) buffer->BufferPointer();
                    const uint8_t** inData = (const uint8_t**) this->decodedFrame->extended_data;

                    int convertedSamplesPerChannel = swr_convert(
                        this->resampler, &outData, samples, inData, samples);

                    if (convertedSamplesPerChannel < 0) {
                        logAvError("swr_convert", convertedSamplesPerChannel);
                        buffer->SetSamples(0);
                    }
                    else {
                        /* actual buffer size, based on resampler output. should be the same
                        as the preferred size... */
                        buffer->SetSamples(convertedSamplesPerChannel * this->channels);
                    }
                }
            }
            else {
                ::debug->Warning(TAG, "avcodec_decode_audio4() failed");
            }

            av_frame_unref(this->decodedFrame);
            return true;
        }
        else {
            logAvError("av_read_frame", errnum);
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
        av_free(this->ioContext);
        this->ioContext = nullptr;
    }
    if (this->codecContext) {
        avcodec_flush_buffers(this->codecContext);

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
    this->streamId = -1;
}

bool FfmpegDecoder::Open(musik::core::sdk::IDataStream *stream) {
    if (stream->Seekable() && this->ioContext == nullptr) {
        ::debug->Info(TAG, "parsing data stream...");

        this->stream = stream;

        this->ioContext = avio_alloc_context(
            this->buffer,
            this->bufferSize,
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
            size_t count = stream->Read(probe, PROBE_SIZE - AVPROBE_PADDING_SIZE);
            stream->SetPosition(0);

            AVProbeData probeData = { 0 };
            probeData.buf = probe;
            probeData.buf_size = count;
            probeData.filename = "";

            this->formatContext->iformat = av_probe_input_format(&probeData, 1);

            if (this->formatContext->iformat) {
                if (avformat_open_input(&this->formatContext, "", nullptr, nullptr) == 0) {
                    if (avformat_find_stream_info(this->formatContext, nullptr) >= 0) {
                        for (unsigned i = 0; i < this->formatContext->nb_streams; i++) {
                            if (this->formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                                this->streamId = (int) i;
                                break;
                            }
                        }
                    }

                    if (this->streamId != -1) {
                        ::debug->Info(TAG, "found audio stream!");
                        this->codecContext = this->formatContext->streams[this->streamId]->codec;
                        if (codecContext) {
                            this->codecContext->request_sample_fmt = AV_SAMPLE_FMT_FLT;
                            AVCodec* codec = avcodec_find_decoder(codecContext->codec_id);
                            if (codec) {
                                if (avcodec_open2(codecContext, codec, nullptr) < 0) {
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
                        }

                        auto stream = this->formatContext->streams[this->streamId];
                        this->rate = stream->codec->sample_rate;
                        this->channels = stream->codec->channels;
                        this->duration = (double) this->formatContext->duration / (double) AV_TIME_BASE;
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