#include "CddaPcmDecoder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

bool CddaPcmDecoder::CanHandle(const mc_media_hint& hint) {
    return hint.extension.length == 3 && hint.extension.data &&
        std::string_view(hint.extension.data, hint.extension.length) == "cda";
}

mc_result CddaPcmDecoder::Open(const mc_stream& stream) {
    mc_stream_info info = MC_STREAM_INFO_DEFAULT;
    const auto result = stream.api->get_info(stream.instance, &info);
    if (result != MC_OK) {
        return result;
    }
    this->input = stream;
    this->lengthKnown = (info.flags & MC_STREAM_LENGTH_KNOWN) != 0;
    this->length = info.length_bytes;
    this->ended = false;
    return MC_OK;
}

mc_result CddaPcmDecoder::Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state) {
    size_t total = 0;
    mc_read_state readState = this->ended ? MC_READ_END : MC_READ_MORE;
    while (total < this->bytes.size() && readState != MC_READ_END) {
        size_t count = 0;
        const auto result = this->input.api->read(
            this->input.instance, this->bytes.data() + total, this->bytes.size() - total, &count, &readState, nullptr);
        if (result != MC_OK) {
            return result;
        }
        if (count > this->bytes.size() - total || (!count && readState != MC_READ_END)) {
            return MC_ERROR_IO;
        }
        total += count;
    }
    if (total % FrameBytes) {
        return MC_ERROR_IO;
    }
    this->ended = readState == MC_READ_END;
    state = this->ended ? MC_DECODE_END : MC_DECODE_MORE;
    const size_t frames = total / FrameBytes;
    return target.Write(Rate, Channels, frames, [&](float* samples, size_t& written) noexcept -> mc_result {
        for (size_t i = 0; i < frames * Channels; ++i) {
            const uint16_t raw = uint16_t(this->bytes[2 * i]) | (uint16_t(this->bytes[2 * i + 1]) << 8);
            const int32_t signedSample = raw < 0x8000 ? raw : static_cast<int32_t>(raw) - 65536;
            samples[i] = static_cast<float>(signedSample) / 32768.0f;
        }
        written = frames;
        return MC_OK;
    });
}

mc_result CddaPcmDecoder::Seek(double seconds, double& actual) {
    if (!std::isfinite(seconds) || seconds < 0 ||
        seconds > static_cast<double>((std::numeric_limits<uint64_t>::max)() / FrameBytes) / Rate)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    const auto frames = static_cast<uint64_t>(seconds * Rate);
    const auto result = this->input.api->seek(this->input.instance, frames * FrameBytes, nullptr);
    if (result == MC_OK) {
        this->ended = false;
        actual = static_cast<double>(frames) / Rate;
    }
    return result;
}

mc_result CddaPcmDecoder::GetDuration(double& seconds) const {
    if (!this->lengthKnown) {
        return MC_ERROR_UNSUPPORTED;
    }
    seconds = static_cast<double>(this->length / FrameBytes) / Rate;
    return MC_OK;
}
