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

#include "FfmpegEncoder.h"
#include <musikcore/plugin/stream.hpp>

#include <algorithm>
#include <random>
#include <map>

#if LIBAVUTIL_VERSION_MAJOR >= 58
#define USE_FFMPEG6_CHANNEL_LAYOUT
#endif

#if LIBAVUTIL_VERSION_MAJOR >= 59
#define USE_FFMPEG7_CHANNEL_LAYOUT
#endif


static const int IO_CONTEXT_BUFFER_SIZE = 4096;
static const int DEFAULT_SAMPLE_RATE = 44100;
static const char* TAG = "FfmpegEncoder";

static std::map<std::string, AVCodecID> formatToCodec = {
    { ".ogg", AV_CODEC_ID_VORBIS },
    { "audio/ogg", AV_CODEC_ID_VORBIS },
    { ".opus", AV_CODEC_ID_OPUS },
    { ".flac", AV_CODEC_ID_FLAC },
    { "audio/flac", AV_CODEC_ID_FLAC },
    { ".alac", AV_CODEC_ID_ALAC },
    { ".aac", AV_CODEC_ID_AAC },
    { "audio/aac", AV_CODEC_ID_AAC },
    { ".aac", AV_CODEC_ID_AAC },
    { ".m4a", AV_CODEC_ID_AAC },
    { ".wma", AV_CODEC_ID_WMAV1 },
    { "audio/x-ms-wma", AV_CODEC_ID_WMAV1 },
    { ".wv", AV_CODEC_ID_WAVPACK },
};

static std::string getAvError(int errnum) {
    char buffer[AV_ERROR_MAX_STRING_SIZE];
    buffer[0] = '\0';
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, errnum);
    return std::string(buffer);
}

void FfmpegEncoder::LogError(const std::string& message) {
    if (logHost.log) {
        logHost.log(logHost.host_context, MC_LOG_WARNING,
            { TAG, sizeof("FfmpegEncoder") - 1 }, { message.data(), message.size() });
    }
}

void FfmpegEncoder::LogAvError(const std::string& method, int errnum) {
    if (errnum != 0) {
        LogError(method + "() failed: " + getAvError(errnum));
    }
}

static AVSampleFormat resolveSampleFormat(AVCodecContext* context, AVCodecCompat *codec) {
#if LIBAVCODEC_VERSION_MAJOR >= 61
    const void* configs = NULL;
    int count = 0;

    int result = avcodec_get_supported_config(
        context,
        codec,
        AV_CODEC_CONFIG_SAMPLE_FORMAT,
        0,
        &configs,
        &count);

    if (result < 0 || count <= 0) {
        return AV_SAMPLE_FMT_NONE;
    }

    const AVSampleFormat* formats = static_cast<const AVSampleFormat*>(configs);
    for (int i = 0; i < count; i++) {
        auto format = formats[i];
        if (format == AV_SAMPLE_FMT_FLT) {
            return format;
        }
    }

    return formats[0];
#else
    if (!codec->sample_fmts) {
        return AV_SAMPLE_FMT_NONE;
    }
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        /* input samples are always AV_SAMPLE_FMT_FLT, so we prefer
        this sample format to minimize resampling */
        if (*p == AV_SAMPLE_FMT_FLT) {
            return *p;
        }
        p++;
    }
    return codec->sample_fmts[0];
#endif
}

static int resolveSampleRate(AVCodecContext* context, AVCodecCompat* codec, int preferredSampleRate) {
#if LIBAVCODEC_VERSION_MAJOR >= 61
    const void* configs = NULL;
    int count = 0;

    int result = avcodec_get_supported_config(
        context,
        codec,
        AV_CODEC_CONFIG_SAMPLE_RATE,
        0,
        &configs,
        &count);

    if (result < 0) {
        return DEFAULT_SAMPLE_RATE;
    }
    /* FFmpeg leaves the list empty when the codec accepts the input rate. */
    if (!configs || count <= 0) {
        return preferredSampleRate;
    }

    int highestRate = 0;
    const int* sampleRates = static_cast<const int*>(configs);
    for (int i = 0; i < count; i++) {
        auto currentRate = sampleRates[i];
        if (currentRate == preferredSampleRate) {
            return preferredSampleRate;
        }
        highestRate = FFMAX(currentRate, highestRate);
    }

    return highestRate;
#else
    const int *p;
    int highestRate = 0;
    if (!codec->supported_samplerates) {
        return preferredSampleRate;
    }
    p = codec->supported_samplerates;
    while (*p) {
        if (*p == preferredSampleRate) {
            return preferredSampleRate;
        }
        highestRate = FFMAX(*p, highestRate);
        p++;
    }
    return highestRate;
#endif
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

#ifdef USE_FFMPEG7_CHANNEL_LAYOUT
static int writeCallback(void* opaque, const uint8_t* buffer, int size) {
#else
static int writeCallback(void* opaque, uint8_t* buffer, int size) {
#endif
    const auto& stream = *static_cast<const mc_stream*>(opaque);
    size_t offset = 0;
    while (offset < static_cast<size_t>(size)) {
        size_t written = 0;
        const size_t remaining = static_cast<size_t>(size) - offset;
        const auto status = stream.api->write(stream.instance, buffer + offset, remaining, &written, nullptr);
        if (status != MC_OK || !written || written > remaining) {
            return AVERROR(EIO);
        }
        offset += written;
    }
    return size;
}

static int64_t seekCallback(void* opaque, int64_t offset, int whence) {
    const auto& stream = *static_cast<const mc_stream*>(opaque);
    whence &= ~AVSEEK_FORCE;
    if (whence == AVSEEK_SIZE) {
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        if (stream.api->get_info(stream.instance, &info) != MC_OK || !(info.flags & MC_STREAM_LENGTH_KNOWN) ||
            info.length_bytes > INT64_MAX)
        {
            return AVERROR(ENOSYS);
        }
        return static_cast<int64_t>(info.length_bytes);
    }
    int64_t actual = 0;
    return musik::core::plugin::seekStream(stream, offset, whence, actual) == MC_OK ? actual : AVERROR(EINVAL);
}

FfmpegEncoder::FfmpegEncoder(const std::string& format, const mc_host_api& host)
: logHost(host)
, format(format) {
    this->isValid = false;
    this->resampler = nullptr;
    this->outputContext = nullptr;
    this->outputCodec = nullptr;
    this->outputFrame = nullptr;
    this->resampledFrame = nullptr;
    this->ioContext = nullptr;
    this->outputFormatContext = nullptr;
    this->outputFifo = nullptr;
    this->globalTimestamp = 0LL;
    this->inputChannelCount = 0;
    this->inputSampleRate = 0;

    std::transform(
        this->format.begin(),
        this->format.end(),
        this->format.begin(),
        ::tolower);
}

bool FfmpegEncoder::OpenOutputCodec(size_t rate, size_t channels, size_t bitrate) {
    if (!this->ioContext) {
        LogError("ioContext not initialized, cannot open output stream");
        return false;
    }

    this->outputFormatContext = avformat_alloc_context();

    if (!this->outputFormatContext) {
        LogError("avformat_alloc_context");
        return false;
    }

    if (!this->format.size()) {
        LogError("invalid format specified: " + this->format);
        return false;
    }

    if (this->format[0] == '.') {
        const std::string tempFilename = "test" + this->format;
        this->outputFormatContext->oformat = av_guess_format(nullptr, tempFilename.c_str(), nullptr);
    }

    if (!this->outputFormatContext->oformat) {
        this->outputFormatContext->oformat = av_guess_format(nullptr, nullptr, this->format.c_str());
    }

    if (!this->outputFormatContext->oformat) {
        LogError("av_guess_format");
        return false;
    }

    this->outputFormatContext->pb = this->ioContext;

    auto it = formatToCodec.find(this->format);
    if (it == formatToCodec.end()) {
        LogError("no codec for specified input format: " + this->format);
        return false;
    }

    this->outputCodec = avcodec_find_encoder(it->second);
    if (!this->outputCodec) {
        LogError("avcodec_find_encoder");
        return false;
    }

    /* note the docs and examples suggest we don't have to free this manually;
    instead, we just need to make sure we release the codec and output context
    correctly. */
    AVStream* stream = avformat_new_stream(this->outputFormatContext, nullptr);

    if (!stream) {
        LogError("avformat_new_stream");
        return false;
    }

    this->outputContext = avcodec_alloc_context3(this->outputCodec);
    if (!this->outputContext) {
        LogError("avcodec_alloc_context3");
        return false;
    }

#ifdef USE_FFMPEG6_CHANNEL_LAYOUT
    av_channel_layout_default(&this->outputContext->ch_layout, static_cast<int>(channels));
#else
    this->outputContext->channels = (int)channels;
    this->outputContext->channel_layout = resolveChannelLayout(channels);
#endif
    this->outputContext->sample_rate = resolveSampleRate(this->outputContext, this->outputCodec, (int) rate);
    this->outputContext->sample_fmt = resolveSampleFormat(this->outputContext, this->outputCodec);
    this->outputContext->bit_rate = (int64_t) bitrate * 1000;
    this->outputContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    if (this->outputContext->sample_fmt == AV_SAMPLE_FMT_NONE) {
        LogError("invalid sample format resolved.");
        return false;
    }

    /* don't quite understand this bit; apparently it sets the sample rate
    for the container(?) */
    stream->time_base.den = (int) rate;
    stream->time_base.num = 1;

    /* also not clear about this, but it's taken from an ffmpeg example. TODO:
    research what global headers are */
    if (this->outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        this->outputContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    int error = avcodec_open2(
        this->outputContext, this->outputCodec, nullptr);

    if (error < 0) {
        LogAvError("avcodec_open2", error);
        return false;
    }

    /* CAL: totally don't understand this, but if we don't do it, then the output
    header cannot be written, and the call returns -22. ugh. */
    error = avcodec_parameters_from_context(stream->codecpar, this->outputContext);

    if (error < 0) {
        LogAvError("avcodec_parameters_from_context", error);
        return false;
    }

    /* resampler context that will be used to convert the input audio
    sample format to the one recommended by the encoder */
#ifdef USE_FFMPEG6_CHANNEL_LAYOUT
    swr_alloc_set_opts2(
        &this->resampler,
        &this->outputContext->ch_layout,
        this->outputContext->sample_fmt,
        this->outputContext->sample_rate,
        &this->outputContext->ch_layout,
        AV_SAMPLE_FMT_FLT,
        (int)rate,
        0,
        nullptr);
#else
    this->resampler = swr_alloc_set_opts(
        nullptr,
        this->outputContext->channel_layout,
        this->outputContext->sample_fmt,
        this->outputContext->sample_rate,
        this->outputContext->channel_layout,
        AV_SAMPLE_FMT_FLT,
        (int)rate,
        0,
        nullptr);
#endif


    error = swr_init(this->resampler);

    if (error < 0) {
        LogAvError("swr_init", error);
        return false;
    }

    /* fifo buffer that will be used by the encoder */
    this->outputFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, (int) channels, 1);

    if (!this->outputFifo) {
        LogError("av_audio_fifo_alloc");
        return false;
    }

    return true;
}

bool FfmpegEncoder::OpenOutputContext() {
    auto* buffer = static_cast<unsigned char*>(av_malloc(IO_CONTEXT_BUFFER_SIZE));
    if (!buffer) {
        return false;
    }
    this->ioContext = avio_alloc_context(buffer, IO_CONTEXT_BUFFER_SIZE, 1, &this->out, nullptr,
        writeCallback, seekCallback);
    if (!this->ioContext) {
        av_free(buffer);
    }
    return this->ioContext != nullptr;
}

bool FfmpegEncoder::Initialize(const mc_stream& out, size_t rate, size_t channels, size_t bitrate) {
    this->out = out;

    if (this->OpenOutputContext()) {
        if (this->OpenOutputCodec(rate, channels, bitrate)) {
            if (this->WriteOutputHeader()) {
                this->inputChannelCount = (int) channels;
                this->inputSampleRate = (int) rate;
                this->isValid = true;
            }
        }
    }

    if (!this->isValid) {
        this->Cleanup();
    }

    return this->isValid;
}

void FfmpegEncoder::Cleanup() {
    this->isValid = false;
    if (this->outputFrame) {
        av_frame_free(&this->outputFrame);
        this->outputFrame = nullptr;
    }
    if (this->resampledFrame) {
        av_frame_free(&this->resampledFrame);
        this->resampledFrame = nullptr;
    }
    if (this->ioContext) {
        /* FFmpeg can replace the original AVIO buffer. release the buffer it
        actually owns before freeing its context. */
        av_freep(&this->ioContext->buffer);
        avio_context_free(&this->ioContext);
    }
    if (this->outputContext) {
        avcodec_free_context(&this->outputContext);
        this->outputContext = nullptr;
        this->outputCodec = nullptr;
    }
    if (this->outputFormatContext) {
        avformat_free_context(this->outputFormatContext);
        this->outputFormatContext = nullptr;
    }
    if (this->resampler) {
        swr_free(&this->resampler);
        this->resampler = nullptr;
    }
    if (this->outputFifo) {
        av_audio_fifo_free(this->outputFifo);
        this->outputFifo = nullptr;
    }
}

void FfmpegEncoder::Release() {
    this->Cleanup();
    delete this;
}

bool FfmpegEncoder::Encode(const mc_pcm_view& pcm) {
    if (!this->isValid || pcm.frame_count == 0) {
        return false;
    }

    if (this->WriteSamplesToFifo(pcm)) {
        if (this->ReadFromFifoAndWriteToOutput(false)) {
            return true;
        }
    }

    this->isValid = false;
    return false;
}

bool FfmpegEncoder::Finalize() {
    if (!this->ReadFromFifoAndWriteToOutput(true) || !this->WriteOutputTrailer()) {
        return false;
    }
    avio_flush(this->ioContext);
    return this->ioContext->error >= 0;
}

bool FfmpegEncoder::WriteOutputTrailer() {
    int error = av_write_trailer(this->outputFormatContext);
    if (error < 0) {
        LogAvError("av_write_trailer", error);
        return false;
    }
    return true;
}

bool FfmpegEncoder::WriteOutputHeader() {
    int error = avformat_write_header(this->outputFormatContext, nullptr);
    if (error < 0) {
        LogAvError("avformat_write_header", error);
        return false;
    }
    return true;
}

bool FfmpegEncoder::WriteSamplesToFifo(const mc_pcm_view& pcm) {
    const int currentFifoSize = av_audio_fifo_size(this->outputFifo);
    if (pcm.frame_count > static_cast<size_t>(INT_MAX - currentFifoSize) ||
        pcm.format.channels != static_cast<unsigned>(this->inputChannelCount) ||
        pcm.format.sample_rate != static_cast<unsigned>(this->inputSampleRate))
    {
        return false;
    }
    const int samplesPerChannel = static_cast<int>(pcm.frame_count);
    const uint8_t* inData = (const uint8_t*) pcm.data;

    int error = av_audio_fifo_realloc(this->outputFifo, currentFifoSize + samplesPerChannel);

    if (error < 0) {
        LogAvError("av_audio_fifo_realloc", error);
        return false;
    }

    int samplesWritten = error = av_audio_fifo_write(
        this->outputFifo,
        (void**) &inData,
        samplesPerChannel);

    if (samplesWritten != samplesPerChannel) {
        LogError("av_audio_fifo_write wrote incorrect number of samples");
        return false;
    }

    return true;
}

bool FfmpegEncoder::ReadFromFifoAndWriteToOutput(bool drain) {
    int outputFrameSize = this->outputContext->frame_size;
    int error = 0;
    while (
        av_audio_fifo_size(this->outputFifo) >= outputFrameSize ||
        (drain && av_audio_fifo_size(this->outputFifo) > 0)
    ) {
        const int fifoSize = av_audio_fifo_size(this->outputFifo);
        const int frameSize = FFMIN(fifoSize, outputFrameSize);

        this->outputFrame = this->ReallocFrame(
            this->outputFrame,
            AV_SAMPLE_FMT_FLT,
            frameSize,
            this->inputSampleRate);

        if (!this->outputFrame) {
            return false;
        }
        int framesRead = av_audio_fifo_read(
            this->outputFifo, (void **) outputFrame->data, frameSize);

        if (framesRead < frameSize) {
            LogError("av_audio_fifo_read read the incorrect number of samples");
            return false;
        }

        this->resampledFrame = this->ReallocFrame(
            this->resampledFrame,
            this->outputContext->sample_fmt,
            frameSize,
            this->outputContext->sample_rate);

        if (!this->resampledFrame) {
            return false;
        }
        error = swr_convert_frame(
            this->resampler,
            this->resampledFrame,
            this->outputFrame);

        if (error < 0) {
            LogAvError("swr_convert_frame", error);
            return false;
        }

        error = this->SendReceiveAndWriteFrame(this->resampledFrame);
        if (error == AVERROR(EAGAIN)) {
            continue;
        }
        else if (error < 0) {
            return false;
        }
    }

    if (!drain) {
        return true;
    }
    if (!this->FlushResampler()) {
        return false;
    }
    /* receive_packet reports EOF once all delayed output was accepted by
    the muxer. other errors must prevent publication of the cache file. */
    return this->SendReceiveAndWriteFrame(nullptr) == AVERROR_EOF;
}

int FfmpegEncoder::SendReceiveAndWriteFrame(AVFrame* frame) {
    if (frame) {
        frame->pts = this->globalTimestamp;
        this->globalTimestamp += frame->nb_samples;
    }

    int error = avcodec_send_frame(this->outputContext, frame);

    if (error < 0) {
        LogAvError("av_codec_send_frame", error);
    }
    else {
        error = 0;
        while (error >= 0) {
            AVPacket outputPacket;
            memset(&outputPacket, 0, sizeof(AVPacket));
            outputPacket.pts = AV_NOPTS_VALUE;
            outputPacket.dts = AV_NOPTS_VALUE;
            outputPacket.pos = -1;
            error = avcodec_receive_packet(this->outputContext, &outputPacket);
            if (error >= 0) {
                error = av_write_frame(this->outputFormatContext, &outputPacket);
                if (error < 0) {
                    LogAvError("av_write_frame", error);
                }
            }
            else if (error < 0 && error != AVERROR(EAGAIN)) {
                LogAvError("avcodec_receive_packet", error);
            }
            av_packet_unref(&outputPacket);
        }
    }

    return error;
}

bool FfmpegEncoder::FlushResampler() {
    int64_t bufferedFrames = swr_get_delay(
        this->resampler, this->outputContext->sample_rate);

    while (bufferedFrames > 0) {
        this->resampledFrame = this->ReallocFrame(
            this->resampledFrame,
            this->outputContext->sample_fmt,
            FFMIN((int) bufferedFrames, this->outputContext->frame_size),
            this->outputContext->sample_rate);

        if (!this->resampledFrame) {
            return false;
        }
        int converted = swr_convert(
            this->resampler,
            this->resampledFrame->extended_data,
            this->resampledFrame->nb_samples,
            nullptr,
            0);

        if (converted > 0) {
            this->resampledFrame->nb_samples = converted;
            if (this->SendReceiveAndWriteFrame(this->resampledFrame) != AVERROR(EAGAIN)) {
                return false;
            }
            bufferedFrames -= converted;
        }
        else {
            return converted == 0;
        }
    }
    return true;
}

AVFrame* FfmpegEncoder::ReallocFrame(
    AVFrame* original,
    AVSampleFormat format,
    int samplesPerChannel,
    int sampleRate)
{
    if (!original || original->nb_samples != samplesPerChannel) {
        if (original) {
            av_frame_free(&original);
        }
        original = av_frame_alloc();
        if (!original) {
            return nullptr;
        }
        original->nb_samples = samplesPerChannel;
        original->format = format;
        original->sample_rate = sampleRate;
#ifdef USE_FFMPEG6_CHANNEL_LAYOUT
        if (av_channel_layout_copy(&original->ch_layout, &this->outputContext->ch_layout) < 0) {
            av_frame_free(&original);
            return nullptr;
        }
#else
        original->channel_layout = this->outputContext->channel_layout;
#endif
        int error = av_frame_get_buffer(original, 0);
        if (error < 0) {
            LogAvError("av_frame_get_buffer", error);
            av_frame_free(&original);
            return nullptr;
        }
    }
    return original;
}
