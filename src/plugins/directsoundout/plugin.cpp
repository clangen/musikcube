#include "DirectSoundOutput.h"
#include <musikcore/plugin/registration.hpp>

namespace {
    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterOutput<DirectSoundOutput>(*host);
    }
    void MC_CALL Shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) return result;
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "731ad687-c52d-47b0-90b4-5483399640b5", 36 };
    plugin->name = { "DirectSound", 11 };
    plugin->version = { "1", 1 };
    plugin->author = { "clangen", 7 };
    plugin->initialize = Initialize;
    plugin->shutdown = Shutdown;
    return MC_OK;
}
