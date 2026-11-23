#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include "../../support/plugins/PluginCandidates.h"

#include <cstdio>
#include <cstdlib>
#include <thread>

using musik::core::ffi::PluginRegistry;

/* assertions remain active in Release; CTest bounds fixture work. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "plugin discovery: %s\n", message);
        std::abort();
    }
}

/* selection gets copied metadata without exposing loading steps. disabled plugins remain listed without running their hooks. */
static void checkDisabled(const std::string& valid) {
    PluginCandidates candidates(fs::u8path(valid).parent_path() / "discovery-runs");
    const auto path = candidates.Add(fs::u8path(valid), "valid");
    unsigned initialized = 0;
    unsigned shutdown = 0;
    PluginRegistry registry([&](mc_log_level, mc_string_view, mc_string_view text) {
        const std::string message(text.data, text.length);
        initialized += message == "initialize";
        shutdown += message == "shutdown";
    });
    require(registry.Initialize(candidates.Directory(),
                [&](const PluginRegistry::PluginInfo& info) {
                    require(info.id == "org.musikcube.fixture", "selection receives identity");
                    require(registry.ListPlugins().empty(), "candidate is unpublished during selection");
                    return false;
                }) == MC_OK,
        "disabled candidate is cataloged");
    const auto catalog = registry.ListPlugins();
    require(catalog.size() == 1 && !catalog.front().enabled && catalog.front().filename == path,
        "disabled metadata remains available after candidate unload");
    require(initialized == 0 && shutdown == 0, "disabled candidate never enters lifecycle hooks");
    require(!registry.Find<mc_output_factory>(catalog.front().id), "disabled plugin cannot supply an output");
    require(registry.Initialize(candidates.Directory()) == MC_ERROR_IN_FLIGHT, "session initializes only once");
}

/* rejection diagnostics identify failures without hiding later valid candidates.
fatal lifecycle reentry has separate subprocess coverage. */
static void checkDiscovery(const std::string& valid, const std::string& failed, const std::string& noEntry) {
    PluginCandidates candidates(fs::u8path(valid).parent_path() / "discovery-runs");
    candidates.Add(fs::u8path(failed), "01-failed");
    candidates.Add(fs::u8path(valid), "02-valid");
    candidates.Add(fs::u8path(noEntry), "03-not-a-plugin");
#ifndef _WIN32
    candidates.AddBrokenLink(fs::u8path(valid));
#endif
    std::vector<std::string> failures;
    PluginRegistry registry([&](mc_log_level, mc_string_view tag, mc_string_view text) {
        if (std::string(tag.data, tag.length) == "PluginRegistry") {
            failures.emplace_back(text.data, text.length);
        }
    });
    require(registry.Initialize(candidates.Directory()) == MC_OK, "optional failures leave session usable");
    require(registry.ListPlugins().size() == 1 && registry.ListPlugins().front().id == "org.musikcube.fixture",
        "only valid candidates are published");
#ifndef _WIN32
    require(failures.size() == 2 && failures.front().find("00-broken") != std::string::npos,
        "broken links are diagnosed without stopping discovery");
#else
    require(failures.size() == 1, "failed candidate is diagnosed");
#endif
    require(failures.back().find("01-failed") != std::string::npos &&
            failures.back().find("provider detail") != std::string::npos,
        "copied failure identifies the plugin and its diagnostic");
}

/* initialization validates thread and session state before doing I/O. missing
folders are empty sessions; actual directory failures close the failed session. */
static void checkInitialization(const std::string& valid) {
    PluginCandidates candidates(fs::u8path(valid).parent_path() / "discovery-runs");
    PluginRegistry registry;
    std::thread worker([&] {
        require(registry.Initialize(candidates.Directory()) == MC_ERROR_WRONG_THREAD, "worker initialization rejected");
    });
    worker.join();
    require(registry.Initialize("") == MC_ERROR_INVALID_ARGUMENT, "empty path rejected");
    require(registry.Initialize(std::string("bad\0path", 8)) == MC_ERROR_INVALID_ARGUMENT, "embedded NUL rejected");
    require(registry.Initialize(candidates.Directory("absent")) == MC_OK, "missing directory is a valid empty session");
    require(registry.ListPlugins().empty() && !registry.Find<mc_output_factory>("absent"),
        "empty session has no plugins or output");
    require(registry.Initialize(candidates.Directory()) == MC_ERROR_IN_FLIGHT, "running session cannot rescan");
    require(registry.Shutdown() == MC_OK, "empty session shuts down");
    require(registry.Initialize(candidates.Directory()) == MC_ERROR_CANCELLED, "closed session cannot restart");

    PluginRegistry failed;
    const auto file = candidates.Add(fs::u8path(valid), "not-a-directory");
    require(failed.Initialize(file) == MC_ERROR_IO, "file cannot be scanned as a directory");
    require(failed.ListPlugins().empty(), "directory failure publishes no partial catalog");
    require(failed.Initialize(candidates.Directory()) == MC_ERROR_CANCELLED, "failed session requires a new registry");
}

/* throwing selection and logging callbacks cannot escape the registry. bad
selection skips that candidate while another candidate can still initialize. */
static void checkCallbackFailures(const std::string& valid, const std::string& failed) {
    PluginCandidates candidates(fs::u8path(valid).parent_path() / "discovery-runs");
    candidates.Add(fs::u8path(failed), "01-failed");
    candidates.Add(fs::u8path(valid), "02-valid");
    PluginRegistry registry([](mc_log_level, mc_string_view, mc_string_view) {
        throw std::runtime_error("log sink failed");
    });
    require(registry.Initialize(candidates.Directory(),
                [](const auto& info) {
                    if (info.id == "org.musikcube.failure") {
                        throw std::runtime_error("selection failed");
                    }
                    return true;
                }) == MC_OK &&
            registry.ListPlugins().size() == 1,
        "callback failures stay local to a candidate");
}

/* fixtures compile as C and are copied into isolated per-scenario directories. */
int main(int argc, char** argv) {
    require(argc == 4, "expected valid, failed, and missing-entry fixture paths");
    checkDisabled(argv[1]);
    checkDiscovery(argv[1], argv[2], argv[3]);
    checkInitialization(argv[1]);
    checkCallbackFailures(argv[1], argv[2]);
}
