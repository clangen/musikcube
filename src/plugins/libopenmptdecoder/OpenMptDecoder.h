#pragma once

#include <common/DeleteDefaults.h>
#include <musikcore/plugin/decoder.hpp>
#include <libopenmpt/libopenmpt.h>
#include <string>

class OpenMptDecoder {
public:
    explicit OpenMptDecoder(const mc_plugin_services&) {}
    ~OpenMptDecoder();

    MUSIK_DELETE_COPY_AND_MOVE(OpenMptDecoder);

    static const std::string& Prefix();
    static bool SupportsPath(const std::string& path);
    static bool CanHandle(const mc_media_hint& hint);
    mc_result Open(const mc_stream& stream);
    mc_result Decode(musik::core::plugin::DecoderBuffer& target, mc_decode_state& state);
    mc_result Seek(double seconds, double& actual);
    mc_result GetDuration(double& seconds) const;
    mc_result SetPreferredSampleRate(uint32_t) { return MC_ERROR_UNSUPPORTED; }

private:
    openmpt_module* module = nullptr;
};
