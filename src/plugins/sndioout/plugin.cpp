#include "SndioOut.h"
#include <musikcore/plugin/registration.hpp>

namespace {
    const mc_text_setting DeviceSetting { .key = { "device_id", 9 }, .default_value = { "", 0 } };
    const mc_configuration_api Configuration {
        .struct_size = sizeof(mc_configuration_api),
        .integers = nullptr,
        .integer_count = 0,
        .booleans = nullptr,
        .boolean_count = 0,
        .numbers = nullptr,
        .number_count = 0,
        .texts = &DeviceSetting,
        .text_count = 1,
    };

    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        const auto result = musik::core::plugin::RegisterOutput<SndioOut>(*host);
        return result == MC_OK ? musik::core::plugin::RegisterConfiguration(*host, Configuration) : result;
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
    plugin->id = { "13903ab8-b387-4de9-86e3-14ba22057dcd", 36 };
    plugin->name = { "sndio", 5 };
    plugin->version = { "1", 1 };
    plugin->author = { "clangen", 7 };
    plugin->initialize = Initialize;
    plugin->shutdown = Shutdown;
    return MC_OK;
}
