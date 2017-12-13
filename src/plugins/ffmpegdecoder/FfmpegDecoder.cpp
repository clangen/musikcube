//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include <algorithm>

#define BUFFER_SIZE 4096
#define PROBE_SIZE 32768

using namespace musik::core::sdk;

static int readCallback(void* opaque, uint8_t* buffer, int bufferSize) {
    FfmpegDecoder* decoder = static_cast<FfmpegDecoder*>(opaque);
    if (decoder && decoder->Stream()) {
        auto count = decoder->Stream()->Read(buffer, (PositionType) bufferSize);
        return (count > 1) ? count : AVERROR_EOF;
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
    this->bufferSize = FF_INPUT_BUFFER_PADDING_SIZE + BUFFER_SIZE;
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

        if (!av_read_frame(this->formatContext, &this->packet)) {
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

                int decodedSize = av_samples_get_buffer_size(
                    nullptr, channels, samples, inFormat, 1);

                if (decodedSize > 0) {
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

                        swr_init(this->resampler);
                    }

                    uint8_t* outData = (uint8_t*) buffer->BufferPointer();
                    const uint8_t** inData = (const uint8_t**) this->decodedFrame->extended_data;
                    swr_convert(this->resampler, &outData, samples, inData, samples);
                }
                else {
                    buffer->SetSamples(0);
                }
            }

            av_frame_unref(this->decodedFrame);
            return true;
        }
    }
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
    if (this->formatContext) {
        avformat_close_input(&this->formatContext);
        this->formatContext = nullptr;
    }
    if (this->codecContext) {
        avcodec_close(this->codecContext);
        this->codecContext = nullptr;
    }
    this->streamId = -1;
}

bool FfmpegDecoder::Open(musik::core::sdk::IDataStream *stream) {
    if (stream->Seekable() && this->ioContext == nullptr) {
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
            size_t count = stream->Read(probe, PROBE_SIZE);
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
                        this->codecContext = this->formatContext->streams[this->streamId]->codec;
                        //if (codecContext) {
                        //    this->codecContext->request_sample_fmt = AV_SAMPLE_FMT_FLT;
                        //    AVCodec* codec = avcodec_find_decoder(codecContext->codec_id);
                        //    if (codec) {
                        //        if (avcodec_open2(codecContext, codec, nullptr) < 0) {
                        //            goto reset_and_fail;
                        //        }
                        //    }
                        //}

                        auto stream = this->formatContext->streams[this->streamId];
                        this->rate = stream->codec->sample_rate;
                        this->channels = stream->codec->channels;
                        this->duration = (double) this->formatContext->duration / (double) AV_TIME_BASE;
                        return true;
                    }
                }
            }
        }
    }

reset_and_fail:
    this->Reset();
    return false;
}

bool FfmpegDecoder::Exhausted() {
    return false;
}