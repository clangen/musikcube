#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/plugin/decoder.hpp>
#include <gme/gme.h>
#include <array>
#include <string>

class GmeDecoder {
public:
    explicit GmeDecoder(const mc_plugin_services& services)
    : preferences(services.preferences) {}
    ~GmeDecoder();

    MUSIK_DELETE_COPY_AND_MOVE(GmeDecoder);

    static const std::string& Prefix();
    static bool SupportsPath(const std::string& path);
    static bool CanHandle(const mc_media_hint& hint);
    mc_result Open(const mc_stream& stream);
    mc_result Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state);
    mc_result Seek(double seconds, double& actual);
    mc_result GetDuration(double& seconds) const;
    mc_result SetPreferredSampleRate(uint32_t) { return MC_ERROR_UNSUPPORTED; }

private:
    mc_result ConfigureTrack(const std::string& path, int track);

    static constexpr uint32_t Rate = 48000;
    static constexpr size_t Channels = 2;
    static constexpr size_t BlockFrames = 1024;

    const mc_preferences_host_api* preferences;
    Music_Emu* emu = nullptr;
    std::array<short, BlockFrames * Channels> samples;
    int64_t durationFrames = -1;
    int64_t position = 0;
};
