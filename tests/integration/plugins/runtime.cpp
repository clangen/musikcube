#include <musikcore/context.h>
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
    else if (component == "CPluginHost") {
        ++seen.rejected;
    }
}

/* repeated contexts exercise session restart without retaining the C module's
host table or callbacks from the preceding context. no library/audio is needed. */
int main(int argc, char** argv) {
    require(argc == 1, "no ambient runtime configuration arguments");
    const auto utf8 = (std::filesystem::u8path(argv[0]).parent_path() / "plugins").u8string();
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
        options.plugin_directory = { plugins.data(), plugins.size() };
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
                    "initialization diagnostic arrives before deadline");
            }
        }
        require(seen.initialized == 1 && seen.rejected == 2, "optional provider failures leave the valid session usable");
        require(seen.failedInitialize == 1 && seen.failedShutdown == 1, "partial provider state rolled back exactly once");
        require(mc_context_destroy(&context) == MC_OK && !context, "context shuts down provider session cleanly");
        test_host_event_destroy(event);
    }
}
