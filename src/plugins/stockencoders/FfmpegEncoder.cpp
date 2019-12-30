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
#include <algorithm>
#include <random>
#include <map>

using namespace musik::core::sdk;

static const int IO_CONTEXT_BUFFER_SIZE = (4096 * 8) + AV_INPUT_BUFFER_PADDING_SIZE;
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
    { ".mp4", AV_CODEC_ID_MPEG4 },
    { "audio/mp4", AV_CODEC_ID_MPEG4 },
    { ".aac", AV_CODEC_ID_AAC },
    { ".m4a", AV_CODEC_ID_AAC },
    { ".wma", AV_CODEC_ID_WMAV2 },
    { "audio/x-ms-wma", AV_CODEC_ID_WMAV2 },
    { ".wv", AV_CODEC_ID_WAVPACK },
};

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
        /* input samples are always AV_SAMPLE_FMT_FLT, so we prefer
        this sample format to minimize resampling */
        if (*p == AV_SAMPLE_FMT_FLT) {
            return *p;
        }
        p++;
    }
    return codec->sample_fmts[0];
}

static int resolveSampleRate(AVCodec* codec, int preferredSampleRate) {
    const int *p;
    int highestRate = 0;
    if (!codec->supported_samplerates) {
        return DEFAULT_SAMPLE_RATE;
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

FfmpegEncoder::FfmpegEncoder(const std::string& format)
: format(format) {
    this->prefs = nullptr;
    this->isValid = false;
    this->resampler = nullptr;
    this->outputContext = nullptr;
    this->outputCodec = nullptr;
    this->outputFrame = nullptr;
    this->ioContext = nullptr;
    this->ioContextOutputBuffer = nullptr;
    this->outputFormatContext = nullptr;
    this->outputFifo = nullptr;
    this->globalTimestamp = 0LL;
    this->planarDataPtr = nullptr;

    std::transform(
        this->format.begin(),
        this->format.end(),
        this->format.begin(),
        ::tolower);
}

bool FfmpegEncoder::OpenOutputCodec(size_t rate, size_t channels, size_t bitrate) {
    if (!this->ioContext) {
        logError("ioContext not initialized, cannot open output stream");
        return false;
    }

    this->outputFormatContext = avformat_alloc_context();

    if (!this->outputFormatContext) {
        logError("avformat_alloc_context");
        return false;
    }

    if (!this->format.size()) {
        logError("invalid format specified: " + this->format);
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
        logError("av_guess_format");
        return false;
    }

    this->outputFormatContext->pb = this->ioContext;

    auto it = formatToCodec.find(this->format);
    if (it == formatToCodec.end()) {
        logError("no codec for specified input format: " + this->format);
        return false;
    }

    this->outputCodec = avcodec_find_encoder(it->second);
    if (!this->outputCodec) {
        logError("avcodec_find_encoder");
        return false;
    }

    /* note the docs and examples suggest we don't have to free this manually;
    instead, we just need to make sure we release the codec and output context
    correctly. */
    AVStream* stream = avformat_new_stream(this->outputFormatContext, nullptr);

    if (!stream) {
        logError("avformat_new_stream");
        return false;
    }

    this->outputContext = avcodec_alloc_context3(this->outputCodec);
    if (!this->outputContext) {
        logError("avcodec_alloc_context3");
        return false;
    }

    this->outputContext->channels = (int) channels;
    this->outputContext->channel_layout = resolveChannelLayout(channels);
    this->outputContext->sample_rate = resolveSampleRate(this->outputCodec, (int) rate);
    this->outputContext->sample_fmt = resolveSampleFormat(this->outputCodec);
    this->outputContext->bit_rate = (int64_t) bitrate * 1000;
    this->outputContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

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
        logAvError("avcodec_open2", error);
        return false;
    }

    /* CAL: totally don't understand this, but if we don't do it, then the output
    header cannot be written, and the call returns -22. ugh. */
    error = avcodec_parameters_from_context(stream->codecpar, this->outputContext);

    if (error < 0) {
        logAvError("avcodec_parameters_from_context", error);
        return false;
    }

    /* resampler context that will be used to convert the input audio
    sample format to the one recommended by the encoder */
    this->resampler = swr_alloc();

    swr_alloc_set_opts(
        this->resampler,
        this->outputContext->channel_layout,
        this->outputContext->sample_fmt,
        this->outputContext->sample_rate,
        this->outputContext->channel_layout,
        AV_SAMPLE_FMT_FLT,
        (int) rate,
        0,
        nullptr);

    error = swr_init(this->resampler);

    if (error < 0) {
        logAvError("swr_init", error);
        return false;
    }

    /* fifo buffer that will be used by the encoder */
    this->outputFifo = av_audio_fifo_alloc(
        this->outputContext->sample_fmt,
        this->outputContext->channels,
        1);

    if (!this->outputFifo) {
        logError("av_audio_fifo_alloc");
        return false;
    }

    return true;
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

bool FfmpegEncoder::Initialize(IDataStream* out, size_t rate, size_t channels, size_t bitrate) {
    this->out = out;
    this->prefs = env()->GetPreferences("FfmpegEncoder");

    if (this->OpenOutputContext()) {
        if (this->OpenOutputCodec(rate, channels, bitrate)) {
            if (this->WriteOutputHeader()) {
                if (av_sample_fmt_is_planar(this->outputContext->sample_fmt)) {
                    this->planarDataPtr = new uint8_t*[channels];
                }
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
    if (this->outputFrame) {
        av_frame_free(&this->outputFrame);
        this->outputFrame = nullptr;
    }
    if (this->resampler) {
        swr_free(&this->resampler);
        this->resampler = nullptr;
    }
    if (this->outputFifo) {
        av_audio_fifo_free(this->outputFifo);
        this->outputFifo = nullptr;
    }
    delete[] this->planarDataPtr;
    this->planarDataPtr = nullptr;
}

void FfmpegEncoder::Release() {
    if (this->prefs) {
        this->prefs->Release();
        this->prefs = nullptr;
    }
    this->Cleanup();
    delete this;
}

IPreferences* FfmpegEncoder::GetPreferences() {
    return this->prefs;
}

bool FfmpegEncoder::WriteOutputHeader() {
    int error = avformat_write_header(this->outputFormatContext, nullptr);
    if (error < 0) {
        logAvError("avformat_write_header", error);
        return false;
    }
    return true;
}

bool FfmpegEncoder::Encode(const IBuffer* pcm) {
    if (!this->isValid || pcm->Samples() == 0) {
        return false;
    }

    const int totalSamples = pcm->Samples();
    const int samplesPerChannel = totalSamples / pcm->Channels();
    const int outputBytesPerSample = av_get_bytes_per_sample(this->outputContext->sample_fmt);
    const int outputSamplesPerChannel = swr_get_out_samples(this->resampler, samplesPerChannel);
    const int outputSizeBytes = outputBytesPerSample * outputSamplesPerChannel * pcm->Channels();
    this->resampledData.reset(outputSizeBytes);
    uint8_t* outData = (uint8_t*) this->resampledData.data;
    const uint8_t* inData = (const uint8_t*) pcm->BufferPointer();

    uint8_t** outDataPtr = nullptr;
    if (planarDataPtr) {
        /* planar formats contain non-interleaved channel data. we can still store this in
        a contiguous block of memory as handled by DataBuffer, but we need to create an
        array of pointers to the relevant chunks of memory */
        int stride = outputSizeBytes / pcm->Channels();
        for (int i = 0; i < pcm->Channels(); i++) {
            planarDataPtr[i] = this->resampledData.data + (i * stride);
        }
        outDataPtr = this->planarDataPtr;
    }
    else {
        /* non-planar formats just interleave data in the same buffer */
        outDataPtr = &outData;
    }

    // std::cerr << "totalSamples: " << totalSamples << " samplesPerChannel: " << samplesPerChannel << " outputBytesPerSample: " <<
    //     outputBytesPerSample << " outputSamplesPerChannel: " << outputSamplesPerChannel << " totalBytes: " <<
    //     (outputBytesPerSample * outputSamplesPerChannel * pcm->Channels()) << " resampler: " << this->resampler <<
    //     " outData: " << (int64_t) outData << " inData: " << (int64_t) inData << " resampledData.length: " <<
    //     resampledData.length << "\n";

    // uint8_t* outDataChannels[2] = { &outData[0], &outData[this->resampledData.length / 2] };

    // std::cerr << "before: " << outDataChannels[0][0] << " " << outDataChannels[1][0] << "\n";

    int convertedSamplesPerChannel = swr_convert(
        this->resampler,
        outDataPtr,
        samplesPerChannel,
        &inData,
        samplesPerChannel);

    // std::cerr << "after: " << outDataChannels[0][0] << " " << outDataChannels[1][0] << "\n";

    if (convertedSamplesPerChannel != samplesPerChannel) {
        return false;
    }

    int currentFifoSize = av_audio_fifo_size(this->outputFifo);
    int error = av_audio_fifo_realloc(this->outputFifo, currentFifoSize + samplesPerChannel);

    if (error < 0) {
        logAvError("av_audio_fifo_realloc", error);
        isValid = false;
        return false;
    }

    int samplesWritten = error = av_audio_fifo_write(
        this->outputFifo,
        (void**) outDataPtr,
        samplesPerChannel);

    if (samplesWritten != samplesPerChannel) {
        logError("av_audio_fifo_write wrote incorrect number of samples");
        return false;
    }

    if (!this->WriteQueueToOutput(false)) {
        this->isValid = false;
        return false;
    }

    return true;
}

void FfmpegEncoder::Finalize() {
    if (this->WriteQueueToOutput(true)) {
        int error = av_write_trailer(this->outputFormatContext);
        if (error < 0) {
            logAvError("av_write_trailer", error);
        }
    }
}

bool FfmpegEncoder::WriteQueueToOutput(bool drain) {
    int outputFrameSize = this->outputContext->frame_size;
    int error = 0;
    while (
        av_audio_fifo_size(this->outputFifo) >= outputFrameSize ||
        (drain && av_audio_fifo_size(this->outputFifo) > 0)
    ) {
        const int fifoSize = av_audio_fifo_size(this->outputFifo);
        const int frameSize = FFMIN(fifoSize, outputFrameSize);

        if (!this->outputFrame || this->outputFrame->nb_samples != frameSize) {
            if (this->outputFrame) {
                av_frame_free(&this->outputFrame);
            }
            this->outputFrame = av_frame_alloc();
            this->outputFrame->nb_samples = frameSize;
            this->outputFrame->channel_layout = this->outputContext->channel_layout;
            this->outputFrame->format = this->outputContext->sample_fmt;
            this->outputFrame->sample_rate = this->outputContext->sample_rate;
            error = av_frame_get_buffer(this->outputFrame, 0);
            if (error < 0) {
                logAvError("av_frame_get_buffer", error);
                isValid = false;
                return false;
            }
        }

        int framesRead = av_audio_fifo_read(
            this->outputFifo, (void **) outputFrame->data, frameSize);

        if (framesRead < frameSize) {
            logError("av_audio_fifo_read read the incorrect number of samples");
            isValid = false;
            return false;
        }

        this->outputFrame->pts = this->globalTimestamp;
        this->globalTimestamp += this->outputFrame->nb_samples;

        error = avcodec_send_frame(this->outputContext, this->outputFrame);
        if (error < 0) {
            logAvError("av_codec_send_frame", error);
            isValid = false;
            return false;
        }

        error = 0;
        while (error >= 0) {
            AVPacket outputPacket;
            av_init_packet(&outputPacket);
            outputPacket.data = nullptr;
            outputPacket.size = 0;
            error = avcodec_receive_packet(this->outputContext, &outputPacket);
            if (error >= 0) {
                error = av_write_frame(this->outputFormatContext, &outputPacket);
                if (error < 0) {
                    logAvError("av_write_frame", error);
                }
            }
            else if (error < 0 && error != AVERROR(EAGAIN)) {
                logAvError("avcodec_receive_packet", error);
            }
            av_packet_unref(&outputPacket);
        }
        if (error == AVERROR(EAGAIN)) {
            continue;
        }
        else if (error < 0) {
            isValid = false;
            return false;
        }
    }

    return true;
}
