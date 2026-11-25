#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include "../../support/plugins/OutputContract.h"

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
    const mc_output_factory* factory = nullptr;

    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "PortAudio output: %s\n", message);
            std::abort();
        }
    }

    mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
        if (std::string_view(capability->id.data, capability->id.length) == MC_PLUGIN_CAPABILITY_OUTPUT) {
            factory = static_cast<const mc_output_factory*>(capability->api);
        }
        return MC_OK;
    }

    void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
    mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
    void MC_CALL destroyError(void*, mc_error**) {}
}

/* the shared contract uses silence against the selected default device. a
machine without audio hardware skips this check rather than changing settings. */
int main(int argc, char** argv) {
    require(argc == 2, "plugin filename is required");
    void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    require(image != nullptr, "load plugin");
    const auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(image, "mc_plugin_get_api"));
    require(entry != nullptr, "find plugin entry");

    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = logMessage;
    host.create_error = createError;
    host.destroy_error = destroyError;
    host.register_plugin_capability = registerCapability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "identify plugin");
    void* state = nullptr;
    require(plugin.initialize(&host, &state, nullptr) == MC_OK && factory, "register output");

    void* instance = nullptr;
    require(factory->create(factory->context, nullptr, &instance, nullptr) == MC_OK && instance, "create output");
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    const auto result = factory->instance_api->get_default_format(instance, {}, &format);
    factory->instance_api->destroy(instance);
    if (result == MC_ERROR_NOT_FOUND || result == MC_ERROR_IO) {
        plugin.shutdown(state);
        dlclose(image);
        return 77;
    }
    require(result == MC_OK && format.sample_rate && format.channels, "get default device format");

    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format = format;
    options.queue_capacity_frames = 128;
    output_contract::Check(*factory, nullptr, options);
    plugin.shutdown(state);
    require(dlclose(image) == 0, "unload plugin");
    return 0;
}
