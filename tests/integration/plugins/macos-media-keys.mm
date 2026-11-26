#include <musikcore/plugin.h>
#include <musikcore/plugin/playback-remote.h>
#include <musikcore/plugin/indexer.h>

#import <AppKit/AppKit.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

namespace {
    const mc_playback_remote_factory* factory = nullptr;

    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "macOS media keys: %s\n", message);
            std::abort();
        }
    }

    mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
        if (capability->id.length == sizeof(MC_PLUGIN_CAPABILITY_PLAYBACK_REMOTE) - 1 &&
            std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_PLAYBACK_REMOTE, capability->id.length) == 0)
        {
            factory = static_cast<const mc_playback_remote_factory*>(capability->api);
        }
        return MC_OK;
    }

    void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
    mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
    void MC_CALL destroyError(void*, mc_error**) {}

    mc_result MC_CALL readState(void*, mc_playback_remote_state_fn, void*) { return MC_ERROR_CANCELLED; }
    mc_result MC_CALL control(void*, mc_playback_command, double, mc_playback_remote_completion_fn, void*) {
        return MC_ERROR_CANCELLED;
    }
    mc_result MC_CALL withPlayback(void*, mc_playback_remote_access_fn, void*) { return MC_ERROR_CANCELLED; }
    mc_result MC_CALL schedule(void*, mc_indexer_scan, mc_playback_remote_completion_fn, void*) {
        return MC_ERROR_CANCELLED;
    }
}

int main(int argc, char** argv) {
    @autoreleasepool {
        require(argc == 2, "plugin path required");
        [NSApplication sharedApplication];
        void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        require(library != nullptr, "plugin loads");
        auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(library, "mc_plugin_get_api"));
        require(entry != nullptr, "C plugin entry exists");

        auto host = mc_host_api MC_HOST_API_DEFAULT;
        host.log = logMessage;
        host.create_error = createError;
        host.destroy_error = destroyError;
        host.register_plugin_capability = registerCapability;
        auto plugin = mc_plugin_api MC_PLUGIN_API_DEFAULT;
        require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "plugin identifies itself");
        void* state = nullptr;
        require(plugin.initialize(&host, &state, nullptr) == MC_OK && factory, "registers playback remote");

        auto remote = mc_plugin_playback_service MC_PLUGIN_PLAYBACK_SERVICE_DEFAULT;
        remote.read_state = readState;
        remote.control_async = control;
        remote.with_playback_async = withPlayback;
        auto indexer = mc_plugin_indexer_service MC_PLUGIN_INDEXER_SERVICE_DEFAULT;
        indexer.schedule_async = schedule;
        auto services = mc_plugin_services MC_PLUGIN_SERVICES_DEFAULT;
        services.host = &host;
        services.playback = &remote;
        services.indexer = &indexer;

        /* immediate stop exercises the handoff between tap startup and thread exit. */
        for (int i = 0; i < 20; ++i) {
            void* instance = nullptr;
            require(factory->create(factory->context, &services, &instance, nullptr) == MC_OK && instance,
                "creates playback remote");
            const auto* api = factory->instance_api;
            const auto result = api->start(instance, nullptr);
            api->request_stop(instance);
            require(api->join(instance, nullptr) == MC_OK, "joins media key tap");
            api->destroy(instance);
            if (result != MC_OK) {
                /* event taps require macOS permissions and can be disabled by the user. */
                require(result == MC_ERROR_IO, "reports unavailable media key tap");
                plugin.shutdown(state);
                dlclose(library);
                return 77;
            }
        }
        plugin.shutdown(state);
        require(dlclose(library) == 0, "unloads after stopping the tap");
        return 0;
    }
}
