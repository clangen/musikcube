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

#include <common/DeleteDefaults.h>
#include <musikcore/plugin/decoder.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

/* owns the demuxer, codec, and resampler for one track. custom AVIO keeps all
file and HTTP reads on the stream chosen by the host. */
class FfmpegDecoder {
public:
    explicit FfmpegDecoder(const mc_plugin_services&) {}
    ~FfmpegDecoder();

    MUSIK_DELETE_COPY_AND_MOVE(FfmpegDecoder);

    static bool CanHandle(const mc_media_hint& hint);
    mc_result Open(const mc_stream& input);
    mc_result Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state);
    mc_result Seek(double seconds, double& actual);
    mc_result GetDuration(double& seconds) const;
    mc_result SetPreferredSampleRate(uint32_t rate);

private:
    static int Read(void* opaque, uint8_t* destination, int capacity);
    static int64_t SeekBytes(void* opaque, int64_t offset, int whence);
    mc_result InitResampler();
    mc_result Convert(musik::core::plugin::DecoderBuffer& target, const AVFrame* input, size_t& count);
    mc_result SendPacket();

    const mc_stream* stream = nullptr;
    AVIOContext* io = nullptr;
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    SwrContext* resampler = nullptr;
    int track = -1;
    int outputRate = 0;
    int inputFormat = AV_SAMPLE_FMT_NONE;
    int inputRate = 0;
    int channels = 0;
    bool draining = false;
    bool inputEnded = false;
    mc_result readError = MC_OK;
};
