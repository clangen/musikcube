#include <musikcore/ffi/plugin_abi/ProviderHost.h>
#include "../../support/plugins/probe.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using musik::core::ffi::ProviderHost;
using namespace std::chrono;

/* fail promptly rather than deadlocking in a destructor after a broken drain
assertion; CTest bounds the whole scenario as a final safeguard. */
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
    ProviderHost::Logger Sink() {
        return [this](mc_log_level level, mc_string_view tag, mc_string_view text) {
            this->Log(level, tag, text);
        };
    }
};

/* use the test-only typed table without exposing or retaining its raw address. */
static mc_result ping(const ProviderHost::Capability& capability) {
    return capability.Invoke([](const void* api, uint32_t size, uint32_t version) {
        require(size == sizeof(test_probe_api) && version == MC_PLUGIN_ABI_VERSION, "typed capability layout/version");
        int32_t value = 0;
        const auto* probe = static_cast<const test_probe_api*>(api);
        const auto result = probe->ping(probe->user_data, &value);
        require(result == MC_OK && value == 42, "call reached pure C provider");
        return result;
    });
}

/* a complete session validates callbacks, duplicate-image rejection, copied IDs,
core-owned diagnostics, owner-thread lifecycle, and one-shot native unload. */
static void checkSession(const std::string& path, const std::string& duplicate) {
    Observations seen;
    ProviderHost host(seen.Sink());
    require(host.Load(path) == MC_OK, "valid C provider loads");
    require(mapped(path), "accepted provider is mapped");
    require(host.Load(path) == MC_ERROR_PLUGIN, "duplicate native image rejected before entry");
    require(seen.negotiate == 1 && seen.initialize == 1, "entry and initialize called once");
    if (!duplicate.empty()) {
        require(host.Load(duplicate) == MC_ERROR_PLUGIN && !mapped(duplicate), "different image with duplicate ID rejected");
        require(seen.negotiate == 2 && seen.initialize == 1, "duplicate ID rejected before initialization");
    }
    auto capability = host.Acquire("org.musikcube.fixture", "probe");
    require(bool(capability), "temporary registration ID copied before return");
    require(ping(capability) == MC_OK, "published capability is callable");
    require(capability.Invoke([&](const void*, uint32_t, uint32_t) {
        return host.Load(path);
    }) == MC_ERROR_IN_FLIGHT,
        "callback cannot recursively load providers");
    require(capability.Invoke([](const void* api, uint32_t, uint32_t) {
        const auto* probe = static_cast<const test_probe_api*>(api);
        return probe->register_again(probe->user_data);
    }) == MC_ERROR_IN_FLIGHT,
        "registration closes after initialize");
    require(capability.Invoke([&](const void*, uint32_t, uint32_t) {
        return host.Shutdown();
    }) == MC_ERROR_IN_FLIGHT,
        "callback cannot deadlock owner-thread shutdown");

    /* rejected worker lifecycle operations leave the owner's live session intact. */
    std::thread worker([&] {
        require(host.Load(path) == MC_ERROR_WRONG_THREAD, "worker load rejected");
        require(host.Shutdown() == MC_ERROR_WRONG_THREAD, "worker shutdown rejected");
        require(capability.Invoke([](const void* api, uint32_t, uint32_t) {
            const auto* probe = static_cast<const test_probe_api*>(api);
            return probe->register_again(probe->user_data);
        }) == MC_ERROR_WRONG_THREAD,
            "worker registration rejected");
    });
    worker.join();
    require(capability.Invoke([](const void*, uint32_t, uint32_t) -> mc_result {
        throw std::runtime_error("provider callback failed");
    }) == MC_ERROR_PLUGIN,
        "callback exceptions contained while image remains mapped");
    require(ping(capability) == MC_OK, "throwing invocation releases call admission");

    /* host-created error storage survives mutation of the source and dlclose. */
    mc_error* detail = nullptr;
    require(capability.Invoke([&](const void* api, uint32_t, uint32_t) {
        const auto* probe = static_cast<const test_probe_api*>(api);
        return probe->make_error(probe->user_data, &detail);
    }) == MC_OK &&
            detail,
        "provider creates an owned host error");
    capability = {};
    require(host.Shutdown() == MC_OK && seen.shutdown == 1, "shutdown runs exactly once");
    require(!mapped(path), "image unmapped after references drain");
    const auto message = mc_error_get_message(detail);
    require(std::string(message.data, message.length) == "provider detail", "error message survives unload");
    mc_error_destroy(&detail);
    require(host.Shutdown() == MC_OK && seen.shutdown == 1, "shutdown is idempotent");
    require(!host.Acquire("org.musikcube.fixture", "probe"), "retired capability cannot be reacquired");
    require(host.Load(path) == MC_ERROR_CANCELLED, "closed host cannot restart implicitly");
}

/* synchronize an executing call with owner shutdown. another thread observes
retirement before releasing the call, proving drain rather than a lucky race. */
static void checkDrain(const std::string& path) {
    Observations seen;
    ProviderHost host(seen.Sink());
    require(host.Load(path) == MC_OK, "drain fixture loads");
    auto capability = host.Acquire("org.musikcube.fixture", "probe");
    std::mutex mutex;
    std::condition_variable gate;
    bool entered = false;
    bool release = false;
    std::atomic<bool> stopped { false };
    std::thread worker([capability = std::move(capability), &mutex, &gate, &entered, &release]() mutable {
        require(capability.Invoke([&](const void* api, uint32_t, uint32_t) {
            /* dropping the original reference inside the call must not unload it. */
            capability = {};
            std::unique_lock<std::mutex> lock(mutex);
            entered = true;
            gate.notify_all();
            require(gate.wait_for(lock, seconds(5), [&] {
                return release;
            }),
                "call released before deadline");
            int32_t value = 0;
            const auto* probe = static_cast<const test_probe_api*>(api);
            return probe->ping(probe->user_data, &value);
        }) == MC_OK,
            "admitted call finishes during shutdown");
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        require(gate.wait_for(lock, seconds(5), [&] {
            return entered;
        }),
            "call entered before shutdown");
    }
    auto retiredLease = host.Acquire("org.musikcube.fixture", "probe");
    std::thread observer([&, retained = std::move(retiredLease)]() mutable {
        /* Acquire has no retirement event; bounded yielding observes that public
        effect without adding a production hook solely for the test. */
        const auto deadline = steady_clock::now() + seconds(5);
        while (host.Acquire("org.musikcube.fixture", "probe")) {
            require(steady_clock::now() < deadline, "registration retires before drain");
            std::this_thread::yield();
        }
        require(ping(retained) == MC_ERROR_CANCELLED, "retirement rejects new calls through old references");
        retained = {};
        require(!stopped && seen.shutdown == 0 && mapped(path), "in-flight call prevents shutdown/unload");
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        gate.notify_all();
    });
    require(host.Shutdown() == MC_OK, "owner shutdown drains admitted call");
    stopped = true;
    worker.join();
    observer.join();
    require(seen.shutdown == 1 && !mapped(path), "final call allows shutdown and native unload");
}

/* two registry instances can map the same native image. their plugin state
and host pointers must remain separate when either session shuts down first. */
static void checkIndependentSessions(const std::string& path) {
    Observations firstSeen;
    Observations secondSeen;
    ProviderHost first(firstSeen.Sink());
    ProviderHost second(secondSeen.Sink());
    require(first.Load(path) == MC_OK && second.Load(path) == MC_OK, "independent provider sessions load");
    auto firstCapability = first.Acquire("org.musikcube.fixture", "probe");
    auto secondCapability = second.Acquire("org.musikcube.fixture", "probe");
    require(ping(firstCapability) == MC_OK && ping(secondCapability) == MC_OK, "both sessions have usable state");
    firstCapability = {};
    require(first.Shutdown() == MC_OK, "first session shuts down");
    require(firstSeen.shutdown == 1 && secondSeen.shutdown == 0, "first host shuts down only its own state");
    require(mapped(path) && ping(secondCapability) == MC_OK, "other session keeps its state and image alive");
    secondCapability = {};
    require(second.Shutdown() == MC_OK && secondSeen.shutdown == 1, "second session shuts down separately");
    require(!mapped(path), "last independent session permits native unload");
}

/* rejected negotiation never invokes lifecycle hooks; failed initialization
rolls back its registrations and invokes shutdown exactly once. */
static void checkRejected(const std::string& path, bool initialized, bool hasEntry, mc_result expected) {
    Observations seen;
    ProviderHost host(seen.Sink());
    mc_error* error = nullptr;
    require(host.Load(path, &error) == expected, "invalid provider reports expected failure");
    require(!mapped(path), "rejected image unloaded");
    require(seen.negotiate == unsigned(hasEntry), "entry-point discovery observed");
    require(seen.initialize == unsigned(initialized) && seen.shutdown == unsigned(initialized), "failed lifecycle rolled back once");
    require(!host.Acquire("org.musikcube.fixture", "probe") &&
        !host.Acquire("org.musikcube.failure", "probe"), "failed registration never published");
    if (initialized && expected == MC_ERROR_PLUGIN) {
        const auto message = mc_error_get_message(error);
        require(std::string(message.data, message.length) == "provider detail", "failure diagnostic survives rollback unload");
    }
    mc_error_destroy(&error);
}

/* each invocation is a fresh process; runtime caches cannot hide missing unload. */
int main(int argc, char** argv) {
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
        FixtureCount
    };
    require(argc == 2 || argc == FixtureCount, "fixture arguments supplied");
    const std::string valid = argv[Valid];
    checkSession(valid, argc == FixtureCount ? argv[DuplicateId] : "");
    checkDrain(valid);
    checkIndependentSessions(valid);
    if (argc == FixtureCount) {
        for (int i = Incompatible; i <= EntryFail; ++i) {
            checkRejected(argv[i], false, true, MC_ERROR_PLUGIN);
        }
        checkRejected(argv[InitFail], true, true, MC_ERROR_PLUGIN);
        checkRejected(argv[BadCapability], true, true, MC_ERROR_INVALID_ARGUMENT);
        checkRejected(argv[NoEntry], false, false, MC_ERROR_UNSUPPORTED);
        checkRejected(argv[MissingVersion], false, true, MC_ERROR_PLUGIN);
    }
    /* logger failures are diagnostics failures, not provider lifecycle failures. */
    ProviderHost quiet([](mc_log_level, mc_string_view, mc_string_view) {
        throw std::runtime_error("log sink failed");
    });
    require(quiet.Load(valid) == MC_OK && quiet.Shutdown() == MC_OK, "throwing bootstrap logger contained");
    std::puts("provider negotiation/lifecycle/drain contracts passed");
}
