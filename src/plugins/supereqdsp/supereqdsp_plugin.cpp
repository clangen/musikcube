#include "SuperEqDsp.h"
#include <musikcore/plugin/dsp.hpp>

namespace {
    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterDsp<SuperEqDsp>(*host);
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
    plugin->id = { "6f0ed53b-0f13-4220-9b0a-ca496b6421cc", 36 };
    plugin->name = { "SuperEq IDSP", 12 };
    plugin->version = { "1", 1 };
    plugin->author = { "Naoki Shibata, Alexey Yakovenko, clangen", 40 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
