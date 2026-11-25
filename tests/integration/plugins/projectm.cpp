#include <musikcore/plugin.h>
#include <musikcore/plugin/visualizer.h>
#include "../../../src/plugins/projectm/Pcm.h"
#include "../../../src/plugins/projectm/PresetDirectory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

namespace {
    const mc_visualizer_factory* visualizer = nullptr;

    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "projectM: %s\n", message);
            std::abort();
        }
    }

    mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
        if (capability->id.length == sizeof(MC_PLUGIN_CAPABILITY_VISUALIZER) - 1 &&
            std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_VISUALIZER, capability->id.length) == 0)
        {
            visualizer = static_cast<const mc_visualizer_factory*>(capability->api);
        }
        return MC_OK;
    }

    void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {
    }
    mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) {
        return MC_ERROR_INTERNAL;
    }
    void MC_CALL destroyError(void*, mc_error**) {
    }
}

int main(int argc, char** argv) {
    float samples[] = { -0.8f, 0.25f, 0.6f };
    require(ScaleVisualizerPcm(samples, 3, 2.0f) == 0.8f, "report unscaled input peak");
    require(samples[0] == -1.0f && samples[1] == 0.5f && samples[2] == 1.0f, "apply and clip gain");
    require(argc == 2, "expected plugin path");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    require(library != nullptr, "load plugin");
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(library, "mc_plugin_get_api"));
    require(entry != nullptr, "find entry point");
    const auto expectedPresets = std::filesystem::canonical(std::filesystem::path(argv[1]).parent_path() / "projectM");
    require(chdir("/tmp") == 0, "change working directory");
    const auto presets = ProjectMPresetDirectory(reinterpret_cast<void*>(entry));
    require(std::filesystem::canonical(presets) == expectedPresets, "find presets beside plugin");
    require(std::filesystem::exists(presets / "README.md"), "stage bundled preset collection");
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = logMessage;
    host.create_error = createError;
    host.destroy_error = destroyError;
    host.register_plugin_capability = registerCapability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate plugin");
    void* state = nullptr;
    require(plugin.initialize(&host, &state, nullptr) == MC_OK && visualizer, "register visualizer");
    require(visualizer->inputs == MC_VISUALIZER_INPUT_PCM, "request PCM only");
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    void* instance = nullptr;
    require(visualizer->create(visualizer->context, &services, &instance, nullptr) == MC_OK && instance,
        "create visualizer");
#ifdef __APPLE__
    /* the helper must present its first frame before show reports success. */
    require(visualizer->instance_api->show(instance) == MC_OK, "open and render the helper window");
    bool visible = false;
    require(visualizer->instance_api->is_visible(instance, &visible) == MC_OK && visible, "show window");
    float pcm[] { 0.1f, -0.1f, 0.2f, -0.2f };
    auto view = mc_pcm_view MC_PCM_VIEW_DEFAULT;
    view.data = pcm;
    view.frame_count = 2;
    view.format.channels = 2;
    view.format.sample_rate = 44100;
    require(visualizer->instance_api->write_pcm(instance, &view) == MC_OK, "accept PCM");
    require(visualizer->instance_api->hide(instance) == MC_OK, "close helper");
    require(visualizer->instance_api->show(instance) == MC_OK, "reopen helper");
    require(visualizer->instance_api->hide(instance) == MC_OK, "close reopened helper");
#else
    require(visualizer->instance_api->show(instance) == MC_ERROR_IO, "report unavailable OpenGL context");
    bool visible = true;
    require(visualizer->instance_api->is_visible(instance, &visible) == MC_OK && !visible, "remain hidden");
    require(visualizer->instance_api->hide(instance) == MC_OK, "stop failed startup");
#endif
    visualizer->instance_api->destroy(instance);
    plugin.shutdown(state);
    dlclose(library);
}
