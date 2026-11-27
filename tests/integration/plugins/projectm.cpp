#include <musikcore/plugin.h>
#include <musikcore/plugin/visualizer.h>
#include "../../../src/plugins/projectm/Pcm.h"
#include "../../../src/plugins/projectm/PresetDirectory.h"
#include "../../../src/plugins/projectm/PresetSearch.h"
#include "../../../src/plugins/projectm/Settings.h"
#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <atomic>
#include <thread>
#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace {
    const mc_visualizer_factory* visualizer = nullptr;
    std::atomic<unsigned> settingReads { 0 };
    std::atomic<bool> settingsRestored { false };

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
    std::vector<std::string> available;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(presets)) {
        if (entry.path().extension() == ".milk") {
            available.push_back(entry.path().string());
        }
    }
    std::sort(available.begin(), available.end());
    require(available.size() > 1, "fixture has selectable presets");
    ProjectMSettings expected;
    expected.automatic = false;
    expected.controlsVisible = false;
    expected.duration = 73.5f;
    expected.gain = 2.25f;
    expected.width = 700;
    expected.height = 480;
    expected.reduceTransitionWork = false;
    const auto selected = std::filesystem::path(available.back()).lexically_relative(presets).string();
    std::strncpy(expected.preset.data(), selected.c_str(), expected.preset.size() - 1);
    mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT;
    settings.context = &expected;
    settings.read_integer = [](void* data, const mc_integer_setting* setting, int32_t* value) -> mc_result {
        ++settingReads;
        auto& expected = *static_cast<ProjectMSettings*>(data);
        *value = std::string_view(setting->key.data, setting->key.length) == "window_width" ? expected.width
                                                                                            : expected.height;
        return MC_OK;
    };
    settings.read_boolean = [](void* data, mc_string_view key, bool fallback, bool* value) -> mc_result {
        auto& expected = *static_cast<ProjectMSettings*>(data);
        const std::string_view name(key.data, key.length);
        *value = name == "automatic"           ? expected.automatic
            : name == "controls_visible"       ? expected.controlsVisible
            : name == "reduce_transition_work" ? expected.reduceTransitionWork
                                               : fallback;
        return MC_OK;
    };
    settings.read_number = [](void* data, mc_string_view key, double, double* value) -> mc_result {
        auto& expected = *static_cast<ProjectMSettings*>(data);
        *value = std::string_view(key.data, key.length) == "audio_gain" ? expected.gain : expected.duration;
        return MC_OK;
    };
    settings.read_text = [](void* data, mc_string_view key, mc_string_view fallback, mc_setting_text_fn callback,
                             void* user) -> mc_result {
        auto& expected = *static_cast<ProjectMSettings*>(data);
        if (std::string_view(key.data, key.length) == "preset") {
            callback(user, { expected.preset.data(), std::strlen(expected.preset.data()) });
        }
        else {
            callback(user, fallback);
        }
        return MC_OK;
    };
    settings.write_values = [](void* data, const mc_setting_entry* entries, size_t count) -> mc_result {
        auto& expected = *static_cast<ProjectMSettings*>(data);
        bool matches = count == 10;
        for (size_t i = 0; i < count; ++i) {
            const std::string_view key(entries[i].key.data, entries[i].key.length);
            const auto& value = entries[i].value;
            if (key == "automatic") {
                matches &= value.data.boolean == expected.automatic;
            }
            if (key == "controls_visible") {
                matches &= value.data.boolean == expected.controlsVisible;
            }
            if (key == "fullscreen") {
                matches &= value.data.boolean == expected.fullscreen;
            }
            if (key == "reduce_transition_work") {
                matches &= value.data.boolean == expected.reduceTransitionWork;
            }
            if (key == "duration_seconds") {
                matches &= value.data.number == expected.duration;
            }
            if (key == "audio_gain") {
                matches &= value.data.number == expected.gain;
            }
            if (key == "window_width") {
                matches &= value.data.integer == expected.width;
            }
            if (key == "window_height") {
                matches &= value.data.integer == expected.height;
            }
            if (key == "preset") {
                matches &= std::string_view(value.data.text.data, value.data.text.length) == expected.preset.data();
            }
        }
        settingsRestored.store(matches);
        return MC_OK;
    };
    services.settings = &settings;
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
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!settingsRestored.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    require(settingsRestored.load(), "helper restores locked preset and settings before saving its live state");
    require(visualizer->instance_api->hide(instance) == MC_OK, "close helper");
    require(settingReads.load() > 0, "restore renderer settings when opening the window");
    settingsRestored.store(false);
    require(visualizer->instance_api->show(instance) == MC_OK, "reopen helper");
    const auto reopenedDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!settingsRestored.load() && std::chrono::steady_clock::now() < reopenedDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    require(settingsRestored.load(), "reopened helper retains its last settings and preset");
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
