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
#include "FfmpegChannelLayout.h"
#include <musikcore/plugin/stream.hpp>

#include <cmath>
#include <limits>
#include <string_view>

using musik::core::plugin::DecoderBuffer;

/* probe only formats supported by the FFmpeg build shipped with the plugin. */
bool FfmpegDecoder::CanHandle(const mc_media_hint& hint) {
    struct Format {
        std::string_view name;
        AVCodecID codec;
    };
    static constexpr Format supported[] = {
        { "mp3", AV_CODEC_ID_MP3 },
        { "audio/mpeg", AV_CODEC_ID_MP3 },
        { "ogg", AV_CODEC_ID_VORBIS },
        { "audio/ogg", AV_CODEC_ID_VORBIS },
        { "opus", AV_CODEC_ID_OPUS },
        { "flac", AV_CODEC_ID_FLAC },
        { "audio/flac", AV_CODEC_ID_FLAC },
        { "alac", AV_CODEC_ID_ALAC },
        { "aac", AV_CODEC_ID_AAC },
        { "audio/aac", AV_CODEC_ID_AAC },
        { "mp4", AV_CODEC_ID_AAC },
        { "audio/mp4", AV_CODEC_ID_AAC },
        { "m4a", AV_CODEC_ID_AAC },
        { "mpc", AV_CODEC_ID_MUSEPACK8 },
        { "mp+", AV_CODEC_ID_MUSEPACK8 },
        { "mpp", AV_CODEC_ID_MUSEPACK8 },
        { "audio/x-musepack", AV_CODEC_ID_MUSEPACK8 },
        { "ape", AV_CODEC_ID_APE },
        { "audio/monkeys-audio", AV_CODEC_ID_APE },
        { "wv", AV_CODEC_ID_WAVPACK },
        { "wma", AV_CODEC_ID_WMAV2 },
        { "audio/x-ms-wma", AV_CODEC_ID_WMAV2 },
    };
    const auto matches = [&](std::string_view name) {
        return (hint.extension.data && name == std::string_view(hint.extension.data, hint.extension.length)) ||
            (hint.content_type.data && name == std::string_view(hint.content_type.data, hint.content_type.length));
    };
    for (const auto& format : supported) {
        if (matches(format.name) && avcodec_find_decoder(format.codec)) {
            return true;
        }
    }
    return matches("wav") || matches("wave") || matches("aif") || matches("aiff") || matches("audio/wav") ||
        matches("audio/x-wav") || matches("audio/wave") || matches("audio/aiff") || matches("audio/x-aiff");
}

FfmpegDecoder::~FfmpegDecoder() {
    swr_free(&resampler);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec);
    avformat_close_input(&format);
    if (io) {
        av_freep(&io->buffer);
        avio_context_free(&io);
    }
}

int FfmpegDecoder::Read(void* opaque, uint8_t* destination, int capacity) {
    auto& self = *static_cast<FfmpegDecoder*>(opaque);
    if (self.readError != MC_OK) {
        return self.readError == MC_ERROR_CANCELLED ? AVERROR_EXIT : AVERROR(EIO);
    }
    /* AVIO accepts either bytes or EOF. deliver the terminal bytes first;
    the next callback reports EOF without asking the provider again. */
    if (self.inputEnded) {
        return AVERROR_EOF;
    }
    size_t count = 0;
    mc_read_state state = MC_READ_MORE;
    const auto& input = *self.stream;
    const auto status = input.api->read(input.instance, destination, capacity, &count, &state, nullptr);
    if (status != MC_OK || count > static_cast<size_t>(capacity) ||
        (state != MC_READ_MORE && state != MC_READ_END) || (!count && state != MC_READ_END))
    {
        self.readError = status == MC_OK ? MC_ERROR_PLUGIN : status;
        return status == MC_ERROR_CANCELLED ? AVERROR_EXIT : AVERROR(EIO);
    }
    self.inputEnded = state == MC_READ_END;
    return count ? static_cast<int>(count) : AVERROR_EOF;
}

int64_t FfmpegDecoder::SeekBytes(void* opaque, int64_t offset, int whence) {
    auto& self = *static_cast<FfmpegDecoder*>(opaque);
    auto& stream = *self.stream;
    whence &= ~AVSEEK_FORCE;
    if (whence == AVSEEK_SIZE) {
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        if (stream.api->get_info(stream.instance, &info) == MC_OK && (info.flags & MC_STREAM_LENGTH_KNOWN) &&
            info.length_bytes <= INT64_MAX)
        {
            return static_cast<int64_t>(info.length_bytes);
        }
        return AVERROR(ENOSYS);
    }
    int64_t actual;
    const auto status = musik::core::plugin::seekStream(stream, offset, whence, actual);
    if (status != MC_OK) {
        return AVERROR(EINVAL);
    }
    /* a demuxer can also seek inside its own read-ahead buffer. only a real
    seek on the provider changes whether its next read can produce new bytes. */
    self.inputEnded = false;
    return actual;
}

mc_result FfmpegDecoder::Open(const mc_stream& input) {
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    const auto status = input.api->get_info(input.instance, &info);
    if (status != MC_OK) {
        return status;
    }
    if (!(info.flags & MC_STREAM_SEEKABLE)) {
        return MC_ERROR_UNSUPPORTED;
    }
    stream = &input;

    /* FFmpeg probes through the same stream it will decode. this also works
    when a network reader supplies fewer probe bytes than requested. */
    constexpr int ioBytes = 4096;
    auto* bytes = static_cast<unsigned char*>(av_malloc(ioBytes));
    if (!bytes) {
        return MC_ERROR_INTERNAL;
    }
    io = avio_alloc_context(bytes, ioBytes, 0, this, Read, nullptr, SeekBytes);
    if (!io) {
        av_free(bytes);
        return MC_ERROR_INTERNAL;
    }
    format = avformat_alloc_context();
    if (!format) {
        return MC_ERROR_INTERNAL;
    }
    format->pb = io;
    format->flags |= AVFMT_FLAG_CUSTOM_IO;
    if (avformat_open_input(&format, "", nullptr, nullptr) < 0 || avformat_find_stream_info(format, nullptr) < 0) {
        return readError != MC_OK ? readError : MC_ERROR_IO;
    }
    track = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (track < 0) {
        return MC_ERROR_UNSUPPORTED;
    }
    const auto* parameters = format->streams[track]->codecpar;
    auto* implementation = avcodec_find_decoder(parameters->codec_id);
    if (!implementation) {
        return MC_ERROR_UNSUPPORTED;
    }
    codec = avcodec_alloc_context3(implementation);
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!codec || !packet || !frame) {
        return MC_ERROR_INTERNAL;
    }
    if (avcodec_parameters_to_context(codec, parameters) < 0 || avcodec_open2(codec, implementation, nullptr) < 0) {
        return MC_ERROR_IO;
    }
    channels = ffmpeg::channelCount(*codec);
    if (channels <= 0 || codec->sample_rate <= 0) {
        return MC_ERROR_IO;
    }
    if (!outputRate) {
        outputRate = codec->sample_rate;
    }
    return readError;
}

mc_result FfmpegDecoder::GetDuration(double& seconds) const {
    if (format->duration == AV_NOPTS_VALUE || format->duration < 0) {
        return MC_ERROR_UNSUPPORTED;
    }
    seconds = static_cast<double>(format->duration) / AV_TIME_BASE;
    return MC_OK;
}

mc_result FfmpegDecoder::SetPreferredSampleRate(uint32_t rate) {
    if (rate == 0 || rate > INT_MAX || resampler) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    outputRate = static_cast<int>(rate);
    return MC_OK;
}

mc_result FfmpegDecoder::Seek(double seconds, double& actual) {
    const auto ticks = seconds * AV_TIME_BASE;
    if (!std::isfinite(ticks) || ticks < 0 || ticks >= static_cast<double>(INT64_MAX)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    auto& audio = *format->streams[track];
    const auto offset = av_rescale_q(std::llround(ticks), AVRational { 1, AV_TIME_BASE }, audio.time_base);
    const auto start = audio.start_time == AV_NOPTS_VALUE ? 0 : audio.start_time;
    if (offset < 0 || (start > 0 && offset > INT64_MAX - start)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    const auto previousError = readError;
    readError = MC_OK;
    const auto seekStatus = av_seek_frame(format, track, start + offset, AVSEEK_FLAG_ANY);
    if (readError != MC_OK) {
        return readError;
    }
    if (seekStatus < 0) {
        readError = previousError;
        return MC_ERROR_IO;
    }
    /* delayed codec frames and resampler history belong to the old position. */
    avcodec_flush_buffers(codec);
    swr_free(&resampler);
    av_frame_unref(frame);
    av_packet_unref(packet);
    draining = false;
    actual = seconds;
    return MC_OK;
}

mc_result FfmpegDecoder::InitResampler() {
    inputFormat = frame->format;
    inputRate = frame->sample_rate;
    const auto status = ffmpeg::configureResampler(resampler, *frame, outputRate);
    if (status < 0) {
        return status == AVERROR(ENOMEM) ? MC_ERROR_INTERNAL : MC_ERROR_IO;
    }
    return swr_init(resampler) >= 0 ? MC_OK : MC_ERROR_IO;
}

mc_result FfmpegDecoder::Convert(DecoderBuffer& target, const AVFrame* input, size_t& count) {
    const int inputFrames = input ? input->nb_samples : 0;
    const int capacity = resampler ? swr_get_out_samples(resampler, inputFrames) : 0;
    if (capacity < 0) {
        return MC_ERROR_IO;
    }
    return target.Write(outputRate, channels, capacity, [&](float* samples, size_t& written) noexcept -> mc_result {
        if (capacity) {
            auto* dst = reinterpret_cast<uint8_t*>(samples);
            auto src = input ? const_cast<const uint8_t**>(input->extended_data) : nullptr;
            const int frames = swr_convert(resampler, &dst, capacity, src, inputFrames);
            if (frames < 0) {
                return MC_ERROR_IO;
            }
            written = static_cast<size_t>(frames);
        }
        count = written;
        return MC_OK;
    });
}

mc_result FfmpegDecoder::SendPacket() {
    for (;;) {
        av_packet_unref(packet);
        const auto status = av_read_frame(format, packet);
        if (readError != MC_OK) {
            return readError;
        }
        if (status == AVERROR_EOF) {
            draining = true;
            return avcodec_send_packet(codec, nullptr) >= 0 ? MC_OK : MC_ERROR_IO;
        }
        if (status < 0) {
            return MC_ERROR_IO;
        }
        if (packet->stream_index == track) {
            return avcodec_send_packet(codec, packet) >= 0 ? MC_OK : MC_ERROR_IO;
        }
    }
}

mc_result FfmpegDecoder::Decode(DecoderBuffer& target, mc_decode_state& state) {
    for (;;) {
        av_frame_unref(frame);
        const auto received = avcodec_receive_frame(codec, frame);
        if (received == AVERROR(EAGAIN) && !draining) {
            const auto status = SendPacket();
            if (status != MC_OK) {
                return status;
            }
            continue;
        }
        if (received < 0 && received != AVERROR_EOF) {
            return MC_ERROR_IO;
        }
        if (received >= 0) {
            /* the resampler reads its configured channel layout and sample
            format. never feed it differently shaped data after a stream switch. */
            if (ffmpeg::channelCount(*frame) != channels || frame->sample_rate <= 0 ||
                (resampler && (inputFormat != frame->format || inputRate != frame->sample_rate)))
            {
                return MC_ERROR_UNSUPPORTED;
            }
        }
        if (received >= 0 && !resampler) {
            const auto status = InitResampler();
            if (status != MC_OK) {
                return status;
            }
        }

        /* a resampler may consume a short frame before it produces any
        samples. keep decoding until there is PCM or the delay is drained. */
        size_t count = 0;
        const auto status = Convert(target, received == AVERROR_EOF ? nullptr : frame, count);
        if (status != MC_OK) {
            return status;
        }
        if (count || received == AVERROR_EOF) {
            state = count ? MC_DECODE_MORE : MC_DECODE_END;
            return MC_OK;
        }
    }
}
