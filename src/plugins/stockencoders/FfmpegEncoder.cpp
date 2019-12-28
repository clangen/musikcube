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

#include "FfmpegEncoder.h"
#include "shared.h"
#include <random>

#ifdef WIN32
#pragma warning(disable: 4996) /* ignore ffmpeg deprecations; hopefully
    at some point they update their docs and examples so we can use
    the more modern APIs. */
#endif

using namespace musik::core::sdk;

static const int IO_CONTEXT_BUFFER_SIZE = 4096 + AV_INPUT_BUFFER_PADDING_SIZE;
static const int DEFAULT_SAMPLE_RATE = 44100;
static const char* TAG = "FfmpegEncoder";

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

static void logError(const std::string& message) {
    if (::debug) {
        ::debug->Warning(TAG, message.c_str());
    }
}

static AVSampleFormat resolveSampleFormat(AVCodec *codec) {
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == AV_SAMPLE_FMT_FLT || *p == AV_SAMPLE_FMT_FLTP) {
            return *p;
        }
        p++;
    }
    return AV_SAMPLE_FMT_NONE;
}

static int resolveSampleRate(AVCodec* codec, int preferredSampleRate) {
    const int *p;
    int highestRate = 0;
    if (!codec->supported_samplerates) {
        return DEFAULT_SAMPLE_RATE;
    }
    p = codec->supported_samplerates;
    while (*p) {
        if (p && *p == preferredSampleRate) {
            return preferredSampleRate;
        }
        highestRate = FFMAX(*p, highestRate);
        p++;
    }
    return highestRate;
}

static int resolveChannelLayout(size_t channelCount) {
    switch (channelCount) {
        case 1: return AV_CH_LAYOUT_MONO;
        case 2: return AV_CH_LAYOUT_STEREO;
        case 3: return AV_CH_LAYOUT_2POINT1;
        case 4: return AV_CH_LAYOUT_3POINT1;
        case 5: return AV_CH_LAYOUT_4POINT1;
        case 6: return AV_CH_LAYOUT_5POINT1;
        default: return AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
}

static int readCallback(void* opaque, uint8_t* buffer, int bufferSize) {
    FfmpegEncoder* encoder = static_cast<FfmpegEncoder*>(opaque);
    if (encoder && encoder->Stream()) {
        auto count = encoder->Stream()->Read(buffer, (PositionType) bufferSize);
        return (count > 1) ? count : AVERROR_EOF;
    }
    return 0;
}

static int writeCallback(void* opaque, uint8_t* buffer, int bufferSize) {
    FfmpegEncoder* encoder = static_cast<FfmpegEncoder*>(opaque);
    if (encoder && encoder->Stream()) {
        auto count = encoder->Stream()->Write(buffer, (PositionType) bufferSize);
        return (count > 1) ? count : AVERROR_EOF;
    }
    return 0;
}

static int64_t seekCallback(void* opaque, int64_t offset, int whence) {
    FfmpegEncoder* encoder = static_cast<FfmpegEncoder*>(opaque);
    if (encoder && encoder->Stream()) {
        IDataStream* stream = encoder->Stream();
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

bool FfmpegEncoder::OpenOutputCodec(size_t rate, size_t channels, size_t bitrate) {
    if (!this->ioContext) {
        logError("ioContext not initialized, cannot open output stream");
        return false;
    }

    this->outputFormatContext = avformat_alloc_context();
    if (!this->outputFormatContext) {
        logError("could not allocate outputFormatContext");
        return false;
    }

    const std::string fn = "test.opus";
    this->outputFormatContext->oformat = av_guess_format(nullptr, fn.c_str(), nullptr);
    if (!this->outputFormatContext->oformat) {
        logError("av_guess_format failed to resolve output codec");
        return false;
    }

    this->outputFormatContext->pb = this->ioContext;

    this->outputCodec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!this->outputCodec) {
        logError("avcodec_find_encoder failed");
        return false;
    }

    /* CAL TODO: examples don't show freeing this stream -- should we? when? */
    AVStream* stream = avformat_new_stream(this->outputFormatContext, nullptr);
    if (!stream) {
        logError("avformat_new_stream failed");
        return false;
    }

    this->outputContext = avcodec_alloc_context3(this->outputCodec);
    if (!this->outputContext) {
        logError("failed to allocate output context");
        return false;
    }

    this->outputContext->channels = (int) channels;
    this->outputContext->channel_layout = resolveChannelLayout(channels);
    this->outputContext->sample_rate = (int) rate;
    this->outputContext->sample_fmt = this->outputCodec->sample_fmts[0];
    this->outputContext->bit_rate = (int64_t) bitrate * 1000;
    this->outputContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    /* don't quite understand this bit; apparently it sets the sample rate
    for the container */
    stream->time_base.den = (int) rate;
    stream->time_base.num = 1;

    /* also not clear about this, but it's taken from an ffmpeg example. TODO:
    research what global headers are */
    if (this->outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        this->outputContext->flags | AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    int error = avcodec_open2(this->outputContext, this->outputCodec, nullptr);
    if (error < 0) {
        logAvError("avcodec_open2", error);
        return false;
    }

    error = avcodec_parameters_from_context(stream->codecpar, this->outputContext);
    if (error < 0) {
        logAvError("avcodec_parameters_from_context", error);
    }

    return true;

    // if (!this->context())

    // this->codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    // if (this->codec) {
    //     this->context = avcodec_alloc_context3(this->codec);
    //     this->context->bit_rate = (int64_t) this->bitrate;
    //     this->context->sample_fmt = resolveSampleFormat(this->codec);
    //     this->context->sample_rate = resolveSampleRate(this->codec, (int) rate);
    //     this->context->channel_layout = resolveChannelLayout(channels);
    //     this->context->channels = av_get_channel_layout_nb_channels(this->context->channel_layout);

    //     AVDictionary *openOptions = nullptr;
    //     av_dict_set(&openOptions, "strict", "experimental", 0);

    //     int result = avcodec_open2(this->context, this->codec, &openOptions);
    //     if (result != 0) {
    //         logAvError("avcodec_open2", result);
    //     }
    //     else {
    //         this->readBufferSize = av_samples_get_buffer_size(
    //             nullptr,
    //             context->channels,
    //             context->frame_size,
    //             context->sample_fmt,
    //             0);

    //         if (this->readBufferSize > 0) {
    //             this->decodedData.reset((size_t) this->readBufferSize);

    //             this->rawFrame = av_frame_alloc();
    //             this->rawFrame->nb_samples = this->context->frame_size;
    //             this->rawFrame->format = AV_SAMPLE_FMT_FLT;
    //             this->rawFrame->channel_layout = this->context->channel_layout;

    //             this->resampledFrame = av_frame_alloc();
    //             this->resampledFrame->nb_samples = this->context->frame_size;
    //             this->resampledFrame->format = this->context->sample_fmt;
    //             this->resampledFrame->channel_layout = this->context->channel_layout;

    //             result = avcodec_fill_audio_frame(
    //                 this->rawFrame,
    //                 this->context->channels,
    //                 this->context->sample_fmt,
    //                 (uint8_t*)this->decodedData.data,
    //                 (int)this->decodedData.length,
    //                 0);

    //             if (result >= 0) {
    //                 this->resampler = swr_alloc();

    //                 swr_alloc_set_opts(
    //                     this->resampler,
    //                     this->context->channel_layout,
    //                     this->context->sample_fmt,
    //                     (int)rate,
    //                     this->context->channel_layout,
    //                     AV_SAMPLE_FMT_FLT,
    //                     (int)rate,
    //                     0,
    //                     nullptr);

    //                 if (swr_init(this->resampler) == 0) {
    //                     return true;
    //                 }
    //             }
    //         }
    //     }

    //     av_dict_free(&openOptions);
    // }

    return false;
}

bool FfmpegEncoder::OpenOutputContext() {
    this->ioContextOutputBuffer = av_malloc(IO_CONTEXT_BUFFER_SIZE);

    this->ioContext = avio_alloc_context(
        reinterpret_cast<unsigned char*>(this->ioContextOutputBuffer),
        IO_CONTEXT_BUFFER_SIZE,
        1,
        this,
        readCallback,
        writeCallback,
        seekCallback);

    return this->ioContext != nullptr;
}

void FfmpegEncoder::Initialize(IDataStream* out, size_t rate, size_t channels, size_t bitrate) {
    avcodec_register_all();

    this->isValid = false;
    this->out = out;
    this->resampler = nullptr;
    this->prefs = env()->GetPreferences("FfmpegEncoder");
    this->outputContext = nullptr;
    this->outputCodec = nullptr;
    this->rawFrame = nullptr;
    this->resampledFrame = nullptr;
    this->ioContext = nullptr;
    this->ioContextOutputBuffer = nullptr;
    this->outputFormatContext = nullptr;

    if (this->OpenOutputContext()) {
        if (this->OpenOutputCodec(rate, channels, bitrate)) {
            this->isValid = true;
        }
    }

    if (!this->isValid) {
        this->Cleanup();
    }
}

void FfmpegEncoder::Cleanup() {
    this->isValid = false;
    if (this->ioContext) {
        av_free(this->ioContext);
        this->ioContext = nullptr;
    }
    if (this->outputContext) {
        avcodec_flush_buffers(this->outputContext);
        avcodec_close(this->outputContext);
        av_free(this->outputContext);
        this->outputContext = nullptr;
        this->outputCodec = nullptr;
    }
    if (this->outputFormatContext) {
        avformat_free_context(this->outputFormatContext);
        this->outputFormatContext = nullptr;
    }
    if (this->ioContextOutputBuffer) {
        av_free(this->ioContextOutputBuffer);
        this->ioContextOutputBuffer = nullptr;
    }
    if (this->rawFrame) {
        av_frame_free(&this->rawFrame);
        this->rawFrame = nullptr;
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

void FfmpegEncoder::Release() {
    this->prefs->Release();
    this->Cleanup();
    delete this;
}

IPreferences* FfmpegEncoder::GetPreferences() {
    return this->prefs;
}

bool FfmpegEncoder::Encode(AVFrame* frame) {
    if (this->isValid) {
        AVPacket packet = { 0 };
        av_init_packet(&packet);

        int didReadPacket = 0;
        int result = 0;

        result = avcodec_send_frame(this->outputContext, this->rawFrame);

        if (result < 0) {
            logAvError("avcodec_send_frame", result);
            this->isValid = false;
        }

        while (result >= 0) {
            result = avcodec_receive_packet(this->outputContext, &packet);
            if (result >= 0) {
                if (packet.size) {
                    this->encodedData.append((char*)packet.data, packet.size);
                }
            }
            else {
                av_free_packet(&packet);
                if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                    return true;
                }
                logAvError("avcodec_receive_packet", result);
                return false;
            }
        }

        av_free_packet(&packet);
        return true;
    }

    return false;
}

bool FfmpegEncoder::Encode(const IBuffer* pcm) {
    // if (!this->isValid) {
    //     return 0;
    // }

    // this->encodedData.reset();

    // int total = pcm->Bytes();
    // if (total > 0) {
    //     int count = (total / this->readBufferSize) + 1;
    //     for (int i = 0; i < count; i++) {
    //         int offset = i * this->readBufferSize;
    //         char* data = ((char*)pcm->BufferPointer()) + offset;
    //         int size = (i == count - 1) ? (pcm->Bytes() - offset) : this->readBufferSize;
    //         this->decodedData.from(data, size);
    //         this->Encode(this->rawFrame);
    //     }
    // }

    // *data = this->encodedData.data;
    // return this->encodedData.length;

    return 0;
}

void FfmpegEncoder::Finalize() {
    // this->encodedData.reset();
    // while (this->Encode(nullptr)) { }
    // *data = this->encodedData.data;
    // return this->encodedData.length;
}
