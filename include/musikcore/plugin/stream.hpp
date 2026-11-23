#pragma once

#include <musikcore/plugin/stream.h>

#include <cstdio>
#include <limits>

namespace musik { namespace core { namespace plugin {
    /* codec libraries express seeks relative to a signed cursor. musikcore
    streams use absolute byte positions and may have an unknown length. */
    inline mc_result seekStream(const mc_stream& stream, int64_t offset, int whence, int64_t& actual) {
        uint64_t base = 0;
        if (whence == SEEK_END) {
            mc_stream_info info = MC_STREAM_INFO_DEFAULT;
            const auto status = stream.api->get_info(stream.instance, &info);
            if (status != MC_OK) {
                return status;
            }
            if (!(info.flags & MC_STREAM_LENGTH_KNOWN)) {
                return MC_ERROR_UNSUPPORTED;
            }
            base = info.length_bytes;
        }
        else if (whence == SEEK_CUR) {
            const auto status = stream.api->tell(stream.instance, &base);
            if (status != MC_OK) {
                return status;
            }
        }
        else if (whence != SEEK_SET) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        constexpr auto maximum = static_cast<uint64_t>((std::numeric_limits<int64_t>::max)());
        if (offset < 0) {
            const auto distance = static_cast<uint64_t>(-(offset + 1)) + 1;
            if (base < distance) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            base -= distance;
        }
        else {
            if (base > maximum - static_cast<uint64_t>(offset)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            base += static_cast<uint64_t>(offset);
        }
        if (base > maximum) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        const auto status = stream.api->seek(stream.instance, base, nullptr);
        if (status == MC_OK) {
            actual = static_cast<int64_t>(base);
        }
        return status;
    }
} } }
