#include "../../support/FatalScope.h"
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/ffi/plugin_abi/Plugin.h>
#include "../../support/plugins/probe.h"
#include "../../support/plugins/PluginCandidates.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using musik::core::ffi::Plugin;
using musik::core::ffi::PluginRegistry;
using musik::core::ffi::PluginCallState;

/* fail in Release too; CTest bounds worker synchronization as a final safeguard. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "provider contract: %s\n", message);
        std::abort();
    }
}

/* probe the native loader, balancing the observational reference immediately. */
static bool mapped(const std::string& path) {
#ifdef _WIN32
    const int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wide[0], size);
    return GetModuleHandleW(wide.c_str()) != nullptr;
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
    if (handle) {
        dlclose(handle);
    }
    return handle != nullptr;
#endif
}

/* diagnostics are copied during delivery, so counts remain valid after unload. */
struct Observations {
    std::atomic<unsigned> negotiate { 0 };
    std::atomic<unsigned> initialize { 0 };
    std::atomic<unsigned> shutdown { 0 };

    /* retain only copied observations; never keep a callback-scoped view. */
    void Log(mc_log_level, mc_string_view, mc_string_view text) {
        const std::string message(text.data, text.length);
        if (message == "negotiate") {
            ++this->negotiate;
        }
        else if (message == "initialize") {
            ++this->initialize;
        }
        else if (message == "shutdown") {
            ++this->shutdown;
        }
    }

    /* bind this test's thread-safe counters to the bootstrap logger. */
    PluginRegistry::Logger Sink() {
        return [this](mc_log_level level, mc_string_view tag, mc_string_view text) {
            this->Log(level, tag, text);
        };
    }
};

/* inspect the test-only registration at the private Plugin seam. arbitrary
capability IDs are not part of the registry's caller-facing lookup API. */
static const test_probe_api& probe(const Plugin& plugin) {
    const auto table = plugin.FindCapability("probe");
    require(table && table->size == sizeof(test_probe_api) && table->version == MC_PLUGIN_ABI_VERSION,
        "fixture registered the expected test table");
    return *static_cast<const test_probe_api*>(table->api);
}

/* validate phases, thread ownership, registration closure, and host-owned error
storage directly on one plugin record without exposing generic registry calls. */
static void checkPluginLifecycle(const std::string& path) {
    Observations seen;
    auto plugin = Plugin::Open(path, seen.Sink());
    require(bool(plugin), "native image opens");
    require(!plugin->FindCapability("probe"), "uninitialized registrations are unavailable");
    require(plugin->Initialize() == MC_ERROR_IN_FLIGHT, "initialize requires identification");
    require(plugin->Identify() == MC_OK && plugin->Identify() == MC_ERROR_IN_FLIGHT, "identify runs once");
    require(plugin->Initialize() == MC_OK && plugin->Initialize() == MC_ERROR_IN_FLIGHT, "initialize runs once");
    require(!plugin->FindCapability("absent"), "missing registrations are unavailable");
    const auto& api = probe(*plugin);
    int32_t value = 0;
    require(api.ping(api.user_data, &value) == MC_OK && value == 42, "pure C registration remains callable");
    require(api.register_again(api.user_data) == MC_ERROR_IN_FLIGHT, "registration closes after initialization");
    std::thread worker([&] {
        require(plugin->Identify() == MC_ERROR_WRONG_THREAD, "worker identify rejected");
        require(plugin->Initialize() == MC_ERROR_WRONG_THREAD, "worker initialize rejected");
        require(plugin->Stop() == MC_ERROR_WRONG_THREAD, "worker stop rejected");
        require(api.register_again(api.user_data) == MC_ERROR_WRONG_THREAD, "worker registration rejected");
    });
    worker.join();
    mc_error* detail = nullptr;
    require(api.make_error(api.user_data, &detail) == MC_OK && detail, "plugin creates host-owned error storage");
    require(plugin->Stop() == MC_OK && !mapped(path) && seen.shutdown == 1, "owner stops and unloads once");
    require(!plugin->FindCapability("probe"), "stopped registrations are unavailable");
    require(plugin->Describe(false).id == "org.musikcube.fixture", "metadata survives native unload");
    const auto message = mc_error_get_message(detail);
    require(std::string(message.data, message.length) == "provider detail", "host error survives native unload");
    mc_error_destroy(&detail);
    require(plugin->Stop() == MC_OK && seen.shutdown == 1, "completed Stop is harmless");
    plugin.reset();
    require(!Plugin::Open(""), "empty native paths are rejected");
}

/* one directory initializes once. copied IDs reject duplicate candidates;
the registry owns its DLL even with no factory views, then unloads on shutdown. */
static void checkSession(const std::string& valid, const std::string& duplicate) {
    PluginCandidates candidates(fs::u8path(valid).parent_path() / "lifecycle-runs");
    const auto path = candidates.Add(fs::u8path(valid), "01-valid");
    if (!duplicate.empty()) {
        candidates.Add(fs::u8path(duplicate), "03-duplicate");
    }
#ifndef _WIN32
    /* a second path to the same image must be rejected before its entry runs. */
    fs::create_symlink(
        fs::u8path(path), fs::u8path(candidates.Directory()) / ("02-alias" + fs::u8path(path).extension().string()));
#endif
    Observations seen;
    unsigned rejected = 0;
    PluginRegistry registry([&](mc_log_level level, mc_string_view tag, mc_string_view text) {
        seen.Log(level, tag, text);
        rejected += std::string(tag.data, tag.length) == "PluginRegistry";
    });
    require(registry.Initialize(candidates.Directory()) == MC_OK, "directory starts a usable session");
    require(registry.ListPlugins().size() == 1 && seen.initialize == 1 && mapped(path), "one candidate is published");
    require(seen.negotiate == (duplicate.empty() ? 1u : 2u), "native aliases are rejected before entry");
    require(rejected >= unsigned(!duplicate.empty()), "rejected duplicates are diagnosed");
    require(!registry.Find<mc_output_factory>("org.musikcube.fixture"), "unrelated capability is not cast as output");
    require(!registry.Find<mc_output_factory>("missing"), "missing plugin gives an empty typed view");
    require(registry.Initialize(candidates.Directory()) == MC_ERROR_IN_FLIGHT, "session cannot reload");
    std::thread worker([&] {
        require(registry.Shutdown() == MC_ERROR_WRONG_THREAD, "worker registry shutdown rejected");
    });
    worker.join();
    require(registry.Shutdown() == MC_OK && seen.shutdown == 1 && !mapped(path), "owner closes session synchronously");
    require(registry.ListPlugins().front().id == "org.musikcube.fixture", "catalog survives session shutdown");
    require(registry.Shutdown() == MC_OK && seen.shutdown == 1, "registry shutdown is idempotent");
    require(registry.Initialize(candidates.Directory()) == MC_ERROR_CANCELLED, "closed registry cannot restart");
}

/* independent sessions may share a native image, but not plugin context state.
closing one registry must leave the other session initialized and mapped. */
static void checkIndependentSessions(const std::string& valid) {
    PluginCandidates candidates(fs::u8path(valid).parent_path() / "lifecycle-runs");
    const auto path = candidates.Add(fs::u8path(valid), "valid");
    Observations firstSeen;
    Observations secondSeen;
    PluginRegistry first(firstSeen.Sink());
    PluginRegistry second(secondSeen.Sink());
    require(first.Initialize(candidates.Directory()) == MC_OK && second.Initialize(candidates.Directory()) == MC_OK,
        "two sessions initialize the same image independently");
    require(first.Shutdown() == MC_OK && firstSeen.shutdown == 1 && secondSeen.shutdown == 0 && mapped(path),
        "first shutdown leaves the other session alive");
    require(
        second.Shutdown() == MC_OK && secondSeen.shutdown == 1 && !mapped(path), "last session closes native image");
}

/* rejected entry tables run no lifecycle hooks; failed initialization cleans
partial state exactly once. diagnostics remain host-owned after rollback. */
static void checkRejected(const std::string& path, bool initialized, bool hasEntry, mc_result expected) {
    Observations seen;
    auto plugin = Plugin::Open(path, seen.Sink());
    require(bool(plugin), "fixture image opens");
    mc_error* error = nullptr;
    auto status = plugin->Identify(&error);
    if (status == MC_OK) {
        status = plugin->Initialize(&error);
    }
    require(status == expected, "invalid plugin reports expected rejection");
    require(!plugin->FindCapability("probe"), "rejected plugin publishes no callable registration");
    require(plugin->Stop() == MC_OK && !mapped(path), "rejected candidate unloads");
    require(seen.negotiate == unsigned(hasEntry), "entry availability observed");
    require(seen.initialize == unsigned(initialized) && seen.shutdown == unsigned(initialized), "rollback runs once");
    if (initialized && expected == MC_ERROR_PLUGIN) {
        const auto message = mc_error_get_message(error);
        require(std::string(message.data, message.length) == "provider detail", "failure error survives rollback");
    }
    mc_error_destroy(&error);
}
/* each owner keeps its own state. nesting and unwinding restore it;
workers never read or mutate another thread's owner-only flag. */
static void checkGuardNesting() {
    PluginCallState calls;
    PluginCallState independent;
    require(!calls.IsActive(), "state begins inactive");
    {
        PluginCallState::Scope outer(calls);
        {
            PluginCallState::Scope inner(calls);
            require(calls.IsActive() && !independent.IsActive(), "nesting affects only this owner");
        }
        require(calls.IsActive(), "inner destruction preserves the outer call");
        std::thread worker([&] {
            PluginCallState::Scope ignored(calls);
            require(!calls.IsActive(), "worker does not inspect owner activity");
            PluginCallState local;
            PluginCallState::Scope scope(local);
            require(local.IsActive(), "worker can own its own lifecycle state");
        });
        worker.join();
        require(calls.IsActive(), "worker scope cannot clear the owner state");
    }
    /* an exception must not poison subsequent owner-thread teardown. */
    try {
        PluginCallState::Scope scope(calls);
        throw std::runtime_error("host failure");
    }
    catch (const std::runtime_error&) {
        require(!calls.IsActive(), "unwinding restores inactive state");
    }
    require(!calls.IsActive(), "last scope restores inactivity");
}

/* returning from a fatal lifecycle operation must fail the child test. */
static constexpr int ShutdownUnexpectedlyReturned = 79;

/* entry/initialize exceptions return contextual errors. shutdown exceptions
must terminate before unload because worker cleanup can no longer be trusted. */
static int checkException(const std::string& stage, const char* path) {
    FatalScope fatalScope;
    auto plugin = Plugin::Open(path, [](mc_log_level, mc_string_view, mc_string_view) {
        std::fputs("fatal-test: unexpected logger\n", stderr);
    });
    require(bool(plugin), "throwing image opens");
    mc_error* error = nullptr;
    auto result = plugin->Identify(&error);
    if (stage != "entry") {
        require(result == MC_OK, "exception fixture identifies");
        result = plugin->Initialize(&error);
    }
    if (stage == "shutdown") {
        require(result == MC_OK, "shutdown fixture initializes");
        plugin->Stop();
        return ShutdownUnexpectedlyReturned;
    }
    require(result == MC_ERROR_PLUGIN, "plugin exception becomes plugin error");
    const auto message = mc_error_get_message(error);
    const std::string text(message.data ? message.data : "", message.length);
    require(text.find(stage == "entry" ? "entry" : "initialization") != std::string::npos &&
            text.find("threw an exception") != std::string::npos && text.find("fixture") != std::string::npos,
        "error identifies the failing plugin operation and exception detail");
    mc_error_destroy(&error);
    require(plugin->Stop() == MC_OK && !mapped(path), "failed plugin cleans up safely");
    return 0;
}

/* enter through actual plugin logging so the assertion protects executing
foreign code, not an artificial test-only call marker. */
static int checkFatalReentry(const std::string& mode, const char* path) {
    FatalScope fatalScope;
    Plugin* active = nullptr;
    auto plugin = Plugin::Open(path, [&](mc_log_level, mc_string_view, mc_string_view text) {
        const std::string message(text.data, text.length);
        if (message != (mode == "plugin-shutdown" ? "shutdown" : "initialize")) {
            return;
        }
        if (mode == "plugin-initialize") {
            active->Initialize();
        }
        else if (mode == "plugin-identify") {
            active->Identify();
        }
        else {
            active->Stop();
        }
    });
    require(bool(plugin), "fatal fixture opens");
    active = plugin.get();
    require(plugin->Identify() == MC_OK, "fatal fixture identifies");
    require(plugin->Initialize() == MC_OK, "fatal fixture initializes unless reentry terminates");
    plugin->Stop();
    return ShutdownUnexpectedlyReturned;
}

/* each invocation is a fresh process; runtime caches cannot hide missing unload. */
int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]).find("--fatal-") == 0) {
        return checkFatalReentry(std::string(argv[1]).substr(sizeof("--fatal-") - 1), argv[2]);
    }
    if (argc == 3 && std::string(argv[1]).find("--exception-") == 0) {
        return checkException(std::string(argv[1]).substr(sizeof("--exception-") - 1), argv[2]);
    }
    checkGuardNesting();
    /* matches the fixture order in tests/plugins.cmake; the external installed
    provider check supplies only Valid. */
    enum Fixture {
        Executable,
        Valid,
        Incompatible,
        NewerMinor,
        Truncated,
        MissingShutdown,
        BadId,
        BadUtf8,
        EntryFail,
        InitFail,
        BadCapability,
        NoEntry,
        DuplicateId,
        MissingVersion,
        BadOutput,
        FixtureCount
    };
    require(argc == 2 || argc == FixtureCount, "fixture arguments supplied");
    const std::string valid = argv[Valid];
    checkPluginLifecycle(valid);
    checkSession(valid, argc == FixtureCount ? argv[DuplicateId] : "");
    checkIndependentSessions(valid);
    if (argc == FixtureCount) {
        for (int i = Incompatible; i <= EntryFail; ++i) {
            checkRejected(argv[i], false, true, MC_ERROR_PLUGIN);
        }
        checkRejected(argv[InitFail], true, true, MC_ERROR_PLUGIN);
        checkRejected(argv[BadCapability], true, true, MC_ERROR_INVALID_ARGUMENT);
        checkRejected(argv[NoEntry], false, false, MC_ERROR_UNSUPPORTED);
        checkRejected(argv[MissingVersion], false, true, MC_ERROR_PLUGIN);
        checkRejected(argv[BadOutput], true, true, MC_ERROR_INVALID_ARGUMENT);
    }
    std::puts("provider identification/session-lifetime contracts passed");
}
