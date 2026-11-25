#include "ProjectMBackend.h"
#include <musikcore/plugin/visualizer.hpp>

namespace {
    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterVisualizer<ProjectMBackend>(*host);
    }

    void MC_CALL Shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "org.musikcube.visualizer.projectm", sizeof("org.musikcube.visualizer.projectm") - 1 };
    plugin->name = { "projectM", 8 };
    plugin->version = { "4.1.8", 5 };
    plugin->initialize = Initialize;
    plugin->shutdown = Shutdown;
    return MC_OK;
}
