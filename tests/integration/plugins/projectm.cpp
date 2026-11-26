#include <musikcore/plugin.h>
#include <musikcore/plugin/visualizer.h>
#include "../../../src/plugins/projectm/Pcm.h"
#include "../../../src/plugins/projectm/PresetDirectory.h"
#include "../../../src/plugins/projectm/PresetSearch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

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
    require(ProjectMPresetMatches("Blue WAVE.milk", "wave"), "match preset names without case sensitivity");
    require(ProjectMPresetMatches("Blue WAVE.milk", ""), "empty search matches every preset");
    require(!ProjectMPresetMatches("Blue WAVE.milk", "orange"), "exclude nonmatching presets");
    float samples[] = { -0.8f, 0.25f, 0.6f };
    ScaleVisualizerPcm(samples, 3, 2.0f);
    require(samples[0] == -1.0f && samples[1] == 0.5f && samples[2] == 1.0f, "apply and clip gain");
    require(argc == 2 || (argc == 3 && std::strcmp(argv[2], "--opengl") == 0), "expected plugin path");
#ifdef _WIN32
    HMODULE library = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    require(library != nullptr, "load plugin");
#ifdef _WIN32
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(GetProcAddress(library, "mc_plugin_get_api"));
#else
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(library, "mc_plugin_get_api"));
#endif
    require(entry != nullptr, "find entry point");
    const auto expectedPresets = std::filesystem::canonical(std::filesystem::path(argv[1]).parent_path() / "projectM");
    std::filesystem::current_path(std::filesystem::temp_directory_path());
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
    const bool opengl = argc == 3;
    require(visualizer->instance_api->show(instance) == (opengl ? MC_OK : MC_ERROR_IO),
        opengl ? "open OpenGL visualizer" : "report unavailable OpenGL context");
    bool visible = true;
    require(visualizer->instance_api->is_visible(instance, &visible) == MC_OK && visible == opengl,
        "report visualizer visibility");
    if (opengl) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    require(visualizer->instance_api->hide(instance) == MC_OK, "stop failed startup");
#endif
    visualizer->instance_api->destroy(instance);
    plugin.shutdown(state);
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
}
