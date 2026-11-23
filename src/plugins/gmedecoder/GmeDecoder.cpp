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

#include "Constants.h"
#include "GmeDecoder.h"
#include <musikcore/plugin/settings.h>
#include <musikcore/plugin/subtrack-source.hpp>
#include <cmath>
#include <memory>

const std::string& GmeDecoder::Prefix() {
    return EXTERNAL_ID_PREFIX;
}
bool GmeDecoder::SupportsPath(const std::string& path) {
    return canHandle(path);
}

bool GmeDecoder::CanHandle(const mc_media_hint& hint) {
    return hint.extension.data && canHandle("." + std::string(hint.extension.data, hint.extension.length));
}

GmeDecoder::~GmeDecoder() {
    if (emu) {
        gme_delete(emu);
    }
}

mc_result GmeDecoder::Open(const mc_stream& stream) {
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    auto status = stream.api->get_info(stream.instance, &info);
    if (status != MC_OK) {
        return status;
    }
    if (!(info.flags & MC_STREAM_LENGTH_KNOWN) || info.length_bytes > LONG_MAX || info.length_bytes > SIZE_MAX) {
        return MC_ERROR_UNSUPPORTED;
    }
    std::vector<char> bytes(static_cast<size_t>(info.length_bytes));
    size_t offset = 0;
    while (offset < bytes.size()) {
        const auto remaining = bytes.size() - offset;
        size_t read = 0;
        mc_read_state state;
        status = stream.api->read(stream.instance, bytes.data() + offset, remaining, &read, &state, nullptr);
        if (status != MC_OK) {
            return status;
        }
        if (!read || read > remaining) {
            return MC_ERROR_IO;
        }
        offset += read;
    }
    if (bytes.empty() || gme_open_data(bytes.data(), static_cast<long>(bytes.size()), &emu, Rate)) {
        return MC_ERROR_IO;
    }
    std::string path;
    int track = 0;
    if (info.uri.data) {
        std::string uri(info.uri.data, info.uri.length);
        if (!musik::core::plugin::SubtrackSource<GmeDecoder>::Parse(uri, path, track)) {
            path = uri;
        }
    }
    return ConfigureTrack(path, track);
}

mc_result GmeDecoder::ConfigureTrack(const std::string& path, int track) {
    bool loop = DEFAULT_ALWAYS_LOOP_FOREVER, ignoreLength = DEFAULT_IGNORE_EMBEDDED_TRACK_LENGTH;
    bool m3u = DEFAULT_ENABLE_M3U;
    double minimum = DEFAULT_MINIMUM_TRACK_LENGTH, duration = DEFAULT_TRACK_LENGTH, fade = DEFAULT_FADE_OUT_LENGTH;
    const auto readBool = [&](const char* key, bool& value) {
        return !preferences ||
            preferences->read_boolean(preferences->context, { key, strlen(key) }, value, &value) == MC_OK;
    };
    const auto readNumber = [&](const char* key, double& value) {
        return !preferences ||
            preferences->read_number(preferences->context, { key, strlen(key) }, value, &value) == MC_OK;
    };
    if (!readBool(KEY_ENABLE_M3U, m3u) || !readBool(KEY_ALWAYS_LOOP_FOREVER, loop) ||
        !readBool(KEY_IGNORE_EMBEDDED_TRACK_LENGTH, ignoreLength) || !readNumber(KEY_MINIMUM_TRACK_LENGTH, minimum) ||
        !readNumber(KEY_DEFAULT_TRACK_LENGTH, duration) || !readNumber(KEY_TRACK_FADE_OUT_LENGTH, fade))
    {
        return MC_ERROR_IO;
    }

    if (m3u) {
        const auto playlist = getM3uFor(path);
        if (!playlist.empty()) {
            gme_load_m3u(emu, playlist.c_str());
        }
    }
    gme_info_t* metadata = nullptr;
    if (gme_track_info(emu, &metadata, track)) {
        return MC_ERROR_IO;
    }
    std::unique_ptr<gme_info_t, decltype(&gme_free_info)> info(metadata, gme_free_info);
    if (gme_start_track(emu, track)) {
        return MC_ERROR_IO;
    }
    const double embedded = info->play_length / 1000.0;
    const bool effect = embedded > 0 && embedded <= minimum;
    if (!effect && loop) {
        return MC_OK;
    }
    if (effect || (info->length != -1 && !ignoreLength)) {
        duration = embedded;
    }
    else {
        const auto fadeStart = std::clamp((duration - fade) * 1000.0, 0.0, static_cast<double>(INT_MAX));
        gme_set_fade(emu, static_cast<int>(fadeStart));
    }
    if (!std::isfinite(duration) || duration < 0 || duration * Rate >= static_cast<double>(INT64_MAX)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    durationFrames = static_cast<int64_t>(duration * Rate);
    return MC_OK;
}

mc_result GmeDecoder::Seek(double seconds, double& actual) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds * 1000 > INT_MAX) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    if (gme_seek(emu, static_cast<int>(seconds * 1000))) {
        return MC_ERROR_IO;
    }
    const auto ms = gme_tell(emu);
    position = static_cast<int64_t>(ms) * Rate / 1000;
    actual = ms / 1000.0;
    return MC_OK;
}

mc_result GmeDecoder::GetDuration(double& seconds) const {
    if (durationFrames < 0) {
        return MC_ERROR_UNSUPPORTED;
    }
    seconds = static_cast<double>(durationFrames) / Rate;
    return MC_OK;
}

mc_result GmeDecoder::Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state) {
    const size_t frames = durationFrames < 0
        ? BlockFrames
        : static_cast<size_t>(std::clamp<int64_t>(durationFrames - position, 0, BlockFrames));
    return target.Write(Rate, Channels, frames, [&](float* dst, size_t& count) noexcept -> mc_result {
        if (frames && gme_play(emu, static_cast<int>(frames * Channels), samples.data())) {
            return MC_ERROR_IO;
        }
        /* only the last partial block may be shorter than our integer scratch. */
        for (size_t i = 0; i < frames * Channels; ++i) {
            dst[i] = static_cast<float>(samples[i]) / SHRT_MAX;
        }
        count = frames;
        position += frames;
        state = durationFrames >= 0 && position >= durationFrames ? MC_DECODE_END : MC_DECODE_MORE;
        return MC_OK;
    });
}
