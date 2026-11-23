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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

namespace ffmpeg {
    /* FFmpeg 5.0 still uses a bitmask. 5.1 introduced AVChannelLayout for
    both decoded frames and codecs, partway through libavutil major 57. */
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    template<typename Audio>
    int channelCount(const Audio& audio) {
        return audio.ch_layout.nb_channels;
    }

    inline int configureResampler(SwrContext*& resampler, const AVFrame& frame, int outputRate) {
        AVChannelLayout layout {};
        /* some PCM files specify only a channel count. let FFmpeg choose the
        standard ordering for that count before it interleaves the samples. */
        if (frame.ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
            av_channel_layout_default(&layout, channelCount(frame));
        }
        else {
            const auto status = av_channel_layout_copy(&layout, &frame.ch_layout);
            if (status < 0) {
                return status;
            }
        }

        const auto status = swr_alloc_set_opts2(&resampler, &layout, AV_SAMPLE_FMT_FLT, outputRate, &layout,
            static_cast<AVSampleFormat>(frame.format), frame.sample_rate, 0, nullptr);
        av_channel_layout_uninit(&layout);
        return status;
    }
#else
    template<typename Audio>
    int channelCount(const Audio& audio) {
        return audio.channels;
    }

    inline int configureResampler(SwrContext*& resampler, const AVFrame& frame, int outputRate) {
        const auto layout = frame.channel_layout
            ? static_cast<int64_t>(frame.channel_layout)
            : av_get_default_channel_layout(channelCount(frame));
        resampler = swr_alloc_set_opts(nullptr, layout, AV_SAMPLE_FMT_FLT, outputRate, layout,
            static_cast<AVSampleFormat>(frame.format), frame.sample_rate, 0, nullptr);
        return resampler ? 0 : AVERROR(ENOMEM);
    }
#endif
}
