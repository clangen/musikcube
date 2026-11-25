#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

namespace {
    const mc_output_factory* output = nullptr;

    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "sndio devices: %s\n", message);
            std::abort();
        }
    }

    mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
        if (capability->id.length == sizeof(MC_PLUGIN_CAPABILITY_OUTPUT) - 1 &&
            std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_OUTPUT, capability->id.length) == 0)
        {
            output = static_cast<const mc_output_factory*>(capability->api);
        }
        return MC_OK;
    }

    void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
    mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
    void MC_CALL destroyError(void*, mc_error**) {}
    void MC_CALL countDevice(void* user, const mc_output_device*) { ++*static_cast<int*>(user); }
}

int main(int argc, char** argv) {
    require(argc == 2, "plugin path supplied");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    require(library, "load plugin");
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(library, "mc_plugin_get_api"));
    require(entry, "plugin entry exists");
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = logMessage;
    host.create_error = createError;
    host.destroy_error = destroyError;
    host.register_plugin_capability = registerCapability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate plugin");
    void* state = nullptr;
    require(plugin.initialize(&host, &state, nullptr) == MC_OK && output, "register output");
    int devices = 0;
    require(output->visit_devices(output->context, countDevice, &devices) == MC_OK && devices == 0,
        "device enumeration does not duplicate the host's default option");
    require(output->read_default_device(output->context, countDevice, &devices) == MC_OK && devices == 1,
        "default device remains discoverable");
    plugin.shutdown(state);
    dlclose(library);
}
