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

#include "OpenMptDecoder.h"
#include "Utility.h"
#include <musikcore/plugin/stream.hpp>
#include <musikcore/plugin/subtrack-source.hpp>
#include <cmath>

const std::string& OpenMptDecoder::Prefix() {
    return EXTERNAL_ID_PREFIX;
}
bool OpenMptDecoder::SupportsPath(const std::string& path) {
    return isFileSupported(path);
}

bool OpenMptDecoder::CanHandle(const mc_media_hint& hint) {
    return hint.extension.data && isFileTypeSupported(std::string(hint.extension.data, hint.extension.length).c_str());
}

OpenMptDecoder::~OpenMptDecoder() {
    if (module) {
        openmpt_module_destroy(module);
    }
}

mc_result OpenMptDecoder::Open(const mc_stream& stream) {
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    auto status = stream.api->get_info(stream.instance, &info);
    if (status != MC_OK) {
        return status;
    }
    std::string path;
    int track = 0;
    if (info.uri.data) {
        musik::core::plugin::SubtrackSource<OpenMptDecoder>::Parse({ info.uri.data, info.uri.length }, path, track);
    }

    /* libopenmpt consumes its input during creation. preserve the first read
    failure because a callback returning zero alone looks like a clean EOF. */
    struct Input {
        const mc_stream& stream;
        mc_result error = MC_OK;
        bool ended = false;
    } input { stream };
    const openmpt_stream_callbacks callbacks {
        [](void* opaque, void* dst, size_t size) -> size_t {
            auto& input = *static_cast<Input*>(opaque);
            if (input.ended || input.error != MC_OK || !size) {
                return 0;
            }
            size_t count = 0;
            mc_read_state state = MC_READ_MORE;
            auto& source = input.stream;
            const auto result = source.api->read(source.instance, dst, size, &count, &state, nullptr);
            if (result != MC_OK || count > size || (state != MC_READ_MORE && state != MC_READ_END) ||
                (!count && state != MC_READ_END))
            {
                input.error = result == MC_OK ? MC_ERROR_PLUGIN : result;
                return 0;
            }
            input.ended = state == MC_READ_END;
            return count;
        },
        [](void* p, int64_t offset, int whence) {
            int64_t actual;
            auto& input = *static_cast<Input*>(p);
            if (musik::core::plugin::seekStream(input.stream, offset, whence, actual) != MC_OK) {
                return -1;
            }
            input.ended = false;
            return 0;
        },
        [](void* p) -> int64_t {
            auto& source = static_cast<Input*>(p)->stream;
            uint64_t position;
            return source.api->tell(source.instance, &position) == MC_OK && position <= INT64_MAX
                ? static_cast<int64_t>(position)
                : -1;
        },
    };
    module = openmpt_module_create2(
        callbacks, &input, openmpt_log_func_silent, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (input.error != MC_OK) {
        return input.error;
    }
    if (!module || track >= openmpt_module_get_num_subsongs(module) || !openmpt_module_select_subsong(module, track)) {
        return MC_ERROR_IO;
    }
    return MC_OK;
}

mc_result OpenMptDecoder::Seek(double seconds, double& actual) {
    if (!std::isfinite(seconds) || seconds < 0) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    actual = openmpt_module_set_position_seconds(module, seconds);
    return actual < 0 ? MC_ERROR_IO : MC_OK;
}

mc_result OpenMptDecoder::GetDuration(double& seconds) const {
    seconds = openmpt_module_get_duration_seconds(module);
    return std::isfinite(seconds) && seconds >= 0 ? MC_OK : MC_ERROR_UNSUPPORTED;
}

mc_result OpenMptDecoder::Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state) {
    constexpr uint32_t rate = 48000;
    constexpr size_t capacity = 2048;
    return target.Write(rate, 2, capacity, [&](float* dst, size_t& count) noexcept -> mc_result {
        count = openmpt_module_read_interleaved_float_stereo(module, rate, capacity, dst);
        state = count ? MC_DECODE_MORE : MC_DECODE_END;
        return MC_OK;
    });
}
