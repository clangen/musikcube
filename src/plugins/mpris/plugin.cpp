#include "mpris.h"

namespace {
    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterPlaybackRemote<MPRISRemote>(*host);
    }
    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "457df67f-f489-415f-975e-282f470b1c10", 36 };
    plugin->name = { "MPRIS interface", 15 };
    plugin->version = { "1", 1 };
    plugin->author = { "brunosmmm", 9 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
