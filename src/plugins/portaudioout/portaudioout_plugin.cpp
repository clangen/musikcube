#include "PortAudioOut.h"

namespace {
    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterOutput<PortAudioOut>(*host);
    }

    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "521934a5-2263-41a4-9852-937d0b63f196", 36 };
    plugin->name = { "PortAudio", 9 };
    plugin->version = { "1", 1 };
    plugin->author = { "clangen", 7 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
