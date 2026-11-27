#include <common/FilesystemPath.h>
#include <musikcore/context.h>
#include <musikcore/plugins.h>
#include "../../support/host-wakeup.h"
#include <chrono>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

/* public-C-API consumer: provider negotiation must work through the actual
shared runtime as well as the private host's focused concurrency tests. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "provider runtime: %s\n", message);
        std::abort();
    }
}

/* all counters are touched only by the owner-thread log callback. */
struct Observations {
    unsigned initialized = 0;
    unsigned failedInitialize = 0;
    unsigned failedShutdown = 0;
    unsigned rejected = 0;
};

/* copy callback-scoped values so the test can check them after reading. */
struct Settings {
    bool enabled = false;
    double seconds = 0;
    std::string label;
    unsigned count = 0;
};

static void MC_CALL recordSetting(void* data, const mc_plugin_setting* setting) {
    auto& settings = *static_cast<Settings*>(data);
    const std::string key(setting->key.data, setting->key.length);
    if (key == "enabled" && setting->value.type == MC_SETTING_BOOLEAN) {
        settings.enabled = setting->value.data.boolean;
        ++settings.count;
    }
    else if (key == "seconds" && setting->value.type == MC_SETTING_NUMBER) {
        settings.seconds = setting->value.data.number;
        ++settings.count;
    }
    else if (key == "label" && setting->value.type == MC_SETTING_TEXT) {
        settings.label.assign(setting->value.data.text.data, setting->value.data.text.length);
        ++settings.count;
    }
}

/* settings are read and written through the same public API as the app UI. */
static void checkSettings() {
    const mc_string_view plugin { "org.musikcube.fixture", 21 };
    Settings defaults;
    require(mc_plugin_read_settings(plugin, recordSetting, &defaults) == MC_OK && defaults.count == 3 &&
        defaults.enabled && defaults.seconds == 2.5 && defaults.label == "default", "read mixed defaults");

    mc_setting_entry entry {};
    entry.key = { "seconds", 7 };
    entry.value.type = MC_SETTING_NUMBER;
    entry.value.data.number = 11.0;
    require(mc_plugin_write_setting(plugin, &entry) == MC_ERROR_INVALID_ARGUMENT, "reject out-of-range setting");
    entry.value.type = MC_SETTING_BOOLEAN;
    entry.value.data.boolean = true;
    require(mc_plugin_write_setting(plugin, &entry) == MC_ERROR_INVALID_ARGUMENT, "reject wrong setting type");
    entry.value.type = MC_SETTING_NUMBER;
    entry.value.data.number = 3.5;
    require(mc_plugin_write_setting(plugin, &entry) == MC_OK, "save number");
    entry.key = { "enabled", 7 };
    entry.value.type = MC_SETTING_BOOLEAN;
    entry.value.data.boolean = false;
    require(mc_plugin_write_setting(plugin, &entry) == MC_OK, "save boolean");
    entry.key = { "label", 5 };
    entry.value.type = MC_SETTING_TEXT;
    entry.value.data.text = { "saved", 5 };
    require(mc_plugin_write_setting(plugin, &entry) == MC_OK, "save text");
    Settings saved;
    require(mc_plugin_read_settings(plugin, recordSetting, &saved) == MC_OK && saved.count == 3 &&
        !saved.enabled && saved.seconds == 3.5 && saved.label == "saved", "read saved mixed settings");
}

/* observe accepted and rejected providers through normal runtime diagnostics;
failed initialization must roll back without preventing a usable context. */
static void MC_CALL logged(void* state, mc_log_level, mc_string_view tag, mc_string_view text) {
    auto& seen = *static_cast<Observations*>(state);
    const std::string component(tag.data ? tag.data : "", tag.length);
    const std::string message(text.data ? text.data : "", text.length);
    if (component == "fixture" && message == "initialize") {
        ++seen.initialized;
    }
    else if (component == "failed-fixture" && message == "initialize") {
        ++seen.failedInitialize;
    }
    else if (component == "failed-fixture" && message == "shutdown") {
        ++seen.failedShutdown;
    }
    else if (component == "PluginRegistry") {
        ++seen.rejected;
    }
}

/* repeated contexts exercise session restart without retaining the C module's
host table or callbacks from the preceding context. no library/audio is needed. */
int main(int argc, char** argv) {
    require(argc == 1, "no ambient runtime configuration arguments");

    /* keep the selected directory distinct from the executable default. a
    restart that silently switches directories must fail to find the fixture. */
    const auto runtime = musik::core::sdk::fs::pathFromUtf8(argv[0]).parent_path();
    const auto selected = runtime / "selected-plugins";
    std::filesystem::rename(runtime / "plugins", selected);
    std::filesystem::create_directory(runtime / "plugins");
    const auto utf8 = selected.u8string();
    const std::string plugins(utf8.begin(), utf8.end());
    for (unsigned run = 0; run < 3; ++run) {
        mc_context* context = nullptr;
        mc_error* error = nullptr;
        Observations seen;
        auto* event = test_host_event_create();
        require(event != nullptr, "host wakeup event created");
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.flags = MC_CONTEXT_ENABLE_LOGGING;

        /* omitted directory on restart must reuse the process's selected
        directory, just as explicitly selecting it again does. */
        if (run != 1) {
            options.plugin_directory = { plugins.data(), plugins.size() };
        }

        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event;
        options.log = logged;
        options.log_user_data = &seen;
        const auto status = mc_context_create(&options, &context, &error);
        if (status != MC_OK) {
            const auto text = mc_error_get_message(error);
            std::fprintf(stderr, "create: %d %.*s\n", status, static_cast<int>(text.length), text.data ? text.data : "");
        }
        require(status == MC_OK, "plugins-only context starts with C provider");
        mc_error_destroy(&error);
        /* logging is asynchronous: one dispatch may precede the log worker.
        wait on the normal runtime wakeup instead of assuming inline delivery. */
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (seen.initialized != 1 || seen.rejected != 2 || seen.failedInitialize != 1 || seen.failedShutdown != 1) {
            require(mc_context_dispatch(context) == MC_OK, "startup log dispatch");
            if (seen.initialized != 1 || seen.rejected != 2 || seen.failedInitialize != 1 || seen.failedShutdown != 1) {
                require(std::chrono::steady_clock::now() < deadline && test_host_event_wait(event),
                    "selected directory fixture initializes after restart before deadline");
            }
        }
        require(seen.initialized == 1 && seen.rejected == 2, "optional provider failures leave the valid session usable");
        require(seen.failedInitialize == 1 && seen.failedShutdown == 1, "partial provider state rolled back exactly once");
        if (run == 0) {
            checkSettings();
        }
        require(mc_context_destroy(&context) == MC_OK && !context, "context shuts down provider session cleanly");
        test_host_event_destroy(event);
    }
}
