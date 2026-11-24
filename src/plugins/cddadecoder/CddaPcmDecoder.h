#pragma once

#include <musikcore/plugin/decoder.hpp>
#include <array>

/* CD audio is interleaved stereo, signed 16-bit PCM at 44.1 kHz. The stream
owns device access; this decoder only interprets its bytes. */
class CddaPcmDecoder {
public:
    explicit CddaPcmDecoder(const mc_plugin_services&) {}

    static bool CanHandle(const mc_media_hint& hint);
    mc_result Open(const mc_stream& stream);
    mc_result Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state);
    mc_result Seek(double seconds, double& actual);
    mc_result GetDuration(double& seconds) const;
    mc_result GetFormat(mc_audio_format& format) const {
        format = { sizeof(mc_audio_format), Rate, static_cast<uint32_t>(Channels), 0 };
        return MC_OK;
    }
    mc_result SetPreferredSampleRate(uint32_t) { return MC_ERROR_UNSUPPORTED; }

private:
    static constexpr uint32_t Rate = 44100;
    static constexpr size_t Channels = 2;
    static constexpr size_t FrameBytes = Channels * sizeof(int16_t);
    static constexpr size_t FramesPerBlock = 1024;
    mc_stream input = MC_STREAM_DEFAULT;
    uint64_t length = 0;
    bool lengthKnown = false;
    bool ended = false;
    std::array<unsigned char, FramesPerBlock * FrameBytes> bytes {};
};
