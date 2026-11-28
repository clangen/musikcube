#include <common/FilesystemPath.h>
#include "../../support/FatalScope.h"
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <type_traits>
#include "../../support/plugins/PluginCandidates.h"
#include "../../support/plugins/OutputContract.h"
#include <musikcore/plugin/output.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <exception>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using musik::core::ffi::PluginRegistry;
using namespace std::chrono;

/* keep even fatal-child fixtures away from the installable plugin tree. */
static fs::path candidateRoot;

/* fail promptly before cleanup can obscure a broken callback/unload contract. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "null output DLL: %s\n", message);
        std::abort();
    }
}

/* check native unload without leaving an extra observational reference alive. */
static bool mapped(const std::string& filename) {
#ifdef _WIN32
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), -1, wide.data(), count);
    return GetModuleHandleW(wide.c_str()) != nullptr;
#else
    void* native = dlopen(filename.c_str(), RTLD_NOW | RTLD_NOLOAD);
    if (native) {
        dlclose(native);
    }
    return native != nullptr;
#endif
}

/* callbacks publish preallocated records without taking locks or allocating.
release/acquire publication makes records readable after the count advances. */
struct Returns {
    struct Record {
        uint64_t token = 0;
        mc_result status = MC_OK;
        steady_clock::time_point completed;
        std::thread::id thread;
    };

    std::array<Record, 64> records;
    std::atomic<size_t> count { 0 };
    std::atomic<unsigned> ready { 0 };

    /* bind a receiver whose lifetime extends through output destruction. */
    mc_output_callbacks Callbacks() {
        return { sizeof(mc_output_callbacks), this,
            [](void* context, uint64_t token, mc_result status) {
                auto& self = *static_cast<Returns*>(context);
                const auto index = self.count.load(std::memory_order_relaxed);
                require(index < self.records.size(), "completion storage is bounded");
                self.records[index] = { token, status, steady_clock::now(), std::this_thread::get_id() };
                self.count.store(index + 1, std::memory_order_release);
            },
            [](void* context) {
                ++static_cast<Returns*>(context)->ready;
            } };
    }
};

/* uniquely owns a DLL-created C instance while the caller holds its registry
capability. neither copying nor moving is allowed; destruction runs in the DLL. */
class Output {
public:
    /* create using only the factory received from mc_plugin_get_api. */
    explicit Output(const mc_output_factory& factory)
    : api(*factory.instance_api) {
        require(factory.create(factory.context, nullptr, &this->instance, nullptr) == MC_OK && this->instance,
            "factory creates an instance in the DLL");
    }

    Output(const Output&) = delete;
    Output(Output&&) = delete;

    /* the host joins producer calls before releasing the instance or its DLL. */
    ~Output() {
        require(this->api.stop(this->instance) == MC_OK, "host stops output before destruction");
        this->api.destroy(this->instance);
    }

    Output& operator=(const Output&) = delete;
    Output& operator=(Output&&) = delete;

    /* one frame per millisecond keeps durations easy to reason about. */
    mc_result Prepare(Returns& returned, size_t capacity = 100) {
        mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
        options.format = { sizeof(mc_audio_format), 1000, 1, 0 };
        options.queue_capacity_frames = capacity;
        const auto callbacks = returned.Callbacks();
        require(this->api.stop(this->instance) == MC_OK, "host stops before prepare");
        return this->api.prepare(this->instance, &options, &callbacks, nullptr);
    }

    /* valid host arguments can still name a device the backend cannot open.
    the owning host stops both the old state and any failed preparation. */
    mc_result PrepareMissingDevice(Returns& returned) {
        mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
        options.format = { sizeof(mc_audio_format), 1000, 1, 0 };
        options.device_id = { "missing", 7 };
        const auto callbacks = returned.Callbacks();
        require(this->api.stop(this->instance) == MC_OK, "host stops before changing device");
        const auto result = this->api.prepare(this->instance, &options, &callbacks, nullptr);
        if (result != MC_OK) {
            require(this->api.stop(this->instance) == MC_OK, "host cleans failed device preparation");
        }
        return result;
    }

    /* samples remain valid until the DLL returns their token. the long case
        lets stop/drain tests cancel deterministically before natural completion. */
    mc_result QueueAudioBuffer(uint64_t token, size_t frames) {
        require(frames <= this->samples.size(), "test PCM storage covers the buffer");
        mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
        buffer.token = token;
        buffer.pcm = { sizeof(mc_pcm_view), this->samples.data(), frames, { sizeof(mc_audio_format), 1000, 1, 0 } };
        return this->api.queue_audio_buffer(this->instance, &buffer);
    }

    /* freeze the worker before checking admission and cancellation. */
    mc_result Pause() { return this->api.pause(this->instance); }

    /* permit producer calls again and notify any paused retry. */
    mc_result Resume() { return this->api.resume(this->instance); }

    /* return accepted ownership synchronously and close buffer. */
    mc_result Stop() { return this->api.stop(this->instance); }

    /* synchronous deliveries leave no device queue to drain. */
    mc_result Drain() { return this->api.drain(this->instance, nullptr); }

    mc_result Timing(mc_output_timing& timing) const { return this->api.get_timing(this->instance, &timing); }

private:
    const mc_output_vtable& api;
    void* instance = nullptr;
    std::array<float, 60000> samples {};
};

/* ordinary pacing needs no injected-clock API: require a minimum duration and
allow a generous completion deadline, so scheduler delays cannot cause failure. */
static void checkPacing(const mc_output_factory& factory) {
    Returns returned;
    Output output(factory);
    require(output.Prepare(returned) == MC_OK, "prepare paced sink");
    const auto submitted = steady_clock::now();
    require(output.QueueAudioBuffer(1, 20) == MC_OK, "buffer accepted");
    require(returned.count == 1, "synchronous sink completes before queue_audio_buffer returns");
    require(output.Drain() == MC_OK, "completed buffer drains");
    require(returned.records[0].token == 1 && returned.records[0].status == MC_OK, "accepted token returns once");
    require(returned.records[0].thread == std::this_thread::get_id(), "completion runs on the calling producer");
    require(returned.records[0].completed - submitted >= milliseconds(20), "sink sleeps for the buffer duration");
    mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
    require(output.Timing(timing) == MC_OK && (timing.valid_flags & MC_OUTPUT_TIMING_ESTIMATED) &&
        timing.frames_played_by_driver == 20 && timing.queued_seconds == 0,
        "the paced clock reaches the accepted PCM tail");
    require(output.Stop() == MC_OK && output.Timing(timing) == MC_OK && timing.valid_flags == 0,
        "stop clears the timing snapshot");
}

/* pause rejects delivery without retaining storage. oversized blocks stay
permanent errors; resume signals that a paused producer can retry. */
static void checkAdmission(const mc_output_factory& factory) {
    Returns returned;
    Output output(factory);
    require(output.Prepare(returned, 10) == MC_OK && output.Pause() == MC_OK, "prepare paused sink");
    require(output.QueueAudioBuffer(10, 11) == MC_ERROR_BUFFER_TOO_SMALL, "oversized input is not retryable");
    require(output.QueueAudioBuffer(11, 6) == MC_ERROR_QUEUE_FULL && returned.count == 0,
        "paused sink leaves the block with its caller");
    require(output.Drain() == MC_OK, "paused sink has no accepted queue to drain");
    require(output.Resume() == MC_OK && returned.ready == 1, "resume wakes a paused retry");
    require(output.QueueAudioBuffer(11, 6) == MC_OK && returned.count == 1, "retry completes synchronously");

    /* repeated calls reuse the same prepared instance, with no pending slot. */
    for (uint64_t token = 20; token < 40; ++token) {
        const auto prior = returned.count.load();
        require(output.QueueAudioBuffer(token, 1) == MC_OK && returned.count == prior + 1,
            "each producer call returns its token before returning");
        require(
            output.Drain() == MC_OK && returned.records[prior].token == token, "completion preserves token identity");
    }
}

/* cancellation is bounded even for a minute of PCM. a start signal precedes
queue entry, so allow either stop-before-admission or accepted-and-cancelled;
the timeout bounds the test rather than claiming an internal admission event. */
static void checkStop(const mc_output_factory& factory) {
    Returns returned;
    Output output(factory);
    require(output.Prepare(returned, 60000) == MC_OK, "prepare a minute-sized block");
    std::promise<void> started;
    auto start = started.get_future();
    auto producer = std::async(std::launch::async, [&] {
        started.set_value();
        return output.QueueAudioBuffer(50, 60000);
    });
    require(start.wait_for(seconds(5)) == std::future_status::ready, "producer begins before cancellation");
    require(producer.wait_for(milliseconds(20)) == std::future_status::timeout, "long delivery has not completed");
    require(output.Stop() == MC_OK, "stop interrupts the producer wait");
    const auto countAtStop = returned.count.load();
    require(producer.wait_for(seconds(5)) == std::future_status::ready, "producer returns after stop");
    const auto status = producer.get();
    require(returned.count == countAtStop, "no completion callback occurs after stop returns");
    if (status == MC_OK) {
        require(
            returned.count == 1 && returned.records[0].token == 50 && returned.records[0].status == MC_ERROR_CANCELLED,
            "accepted delivery returns its token with cancellation");
    }
    else {
        require(status == MC_ERROR_CANCELLED && returned.count == 0, "stop before admission produces no completion");
    }
    require(output.Stop() == MC_OK && output.Drain() == MC_ERROR_CANCELLED, "stop is idempotent and closes drain");
    require(output.QueueAudioBuffer(51, 1) == MC_ERROR_CANCELLED, "stopped sink rejects new deliveries");
}

/* valid arguments may fail at the device boundary. the host owns recovery
and joins producers/stops the instance before destroying it. */
static void checkCleanup(const mc_output_factory& factory) {
    Returns returned;
    {
        Output output(factory);
        require(output.Prepare(returned) == MC_OK, "prepare initial device");
        require(output.QueueAudioBuffer(60, 1) == MC_OK && returned.count == 1, "initial delivery finishes");
        require(
            output.PrepareMissingDevice(returned) == MC_ERROR_INVALID_ARGUMENT, "backend rejects unavailable device");
        require(output.Prepare(returned) == MC_OK, "host can prepare again after failed-device cleanup");
        require(output.QueueAudioBuffer(61, 1) == MC_OK && returned.count == 2, "recovered output completes normally");
        require(output.Pause() == MC_OK, "paused instance can be destroyed by its owner");
    }
    require(returned.count == 2, "host-owned destruction does not duplicate completed tokens");
}

/* typed factory views cannot be forged from raw pointers or invoked using a
callback for a different table. these assertions protect the caller-facing API. */
using OutputFactory = PluginRegistry::Factory<mc_output_factory>;
static_assert(!std::is_constructible_v<OutputFactory, const void*>);
static_assert(std::is_invocable_v<decltype(&OutputFactory::Invoke), const OutputFactory&,
    std::function<mc_result(const mc_output_factory&)>>);
static_assert(!std::is_invocable_v<decltype(&OutputFactory::Invoke), const OutputFactory&,
    std::function<mc_result(const mc_audio_format&)>>);

/* a disabled real output must stay in the catalog without becoming callable.
registry destruction must unload enabled outputs even if passive views survive. */
static void checkRegistryOwnership(const std::string& source) {
    PluginCandidates candidates(candidateRoot / "output-runs");
    const auto path = candidates.Add(musik::core::sdk::fs::pathFromUtf8(source), "nullout");
    {
        PluginRegistry disabled;
        require(disabled.Initialize(candidates.Directory(),
                    [](const auto&) {
                        return false;
                    }) == MC_OK,
            "real output can be disabled");
        const auto catalog = disabled.ListPlugins();
        require(catalog.size() == 1 && !catalog.front().enabled && !mapped(path),
            "disabled output is cataloged and unloaded");
        require(!disabled.Find<mc_output_factory>(catalog.front().id), "disabled output cannot be retrieved");
    }
    OutputFactory retained;
    {
        PluginRegistry registry;
        require(registry.Initialize(candidates.Directory()) == MC_OK, "enabled output session initializes");
        retained = registry.Find<mc_output_factory>(registry.ListPlugins().front().id);
        require(bool(retained) && mapped(path), "view refers to a loaded session output");
    }
    require(!mapped(path), "registry destructor unloads despite a retained passive view");
    retained = {};
    require(!mapped(path), "discarding an expired view performs no plugin work");
}

/* lifecycle reentry must abort before returning or unwinding the session. */
static constexpr int ReentryReturned = 79;

/* exercise real registry entry points, not an artificially set guard. */
static int checkFatalReentry(const std::string& mode, const std::string& source) {
    PluginCandidates candidates(candidateRoot / "reentry-runs");
    candidates.Add(musik::core::sdk::fs::pathFromUtf8(source), "01-output");
    FatalScope fatalScope;
    PluginRegistry* active = nullptr;
    PluginRegistry registry([&](mc_log_level, mc_string_view tag, mc_string_view) {
        if (mode == "diagnostic" && std::string(tag.data, tag.length) == "PluginRegistry") {
            active->Shutdown();
        }
    });
    active = &registry;
    if (mode == "diagnostic") {
        candidates.Add(musik::core::sdk::fs::pathFromUtf8(source), "02-duplicate");
    }
    require(registry.Initialize(candidates.Directory(),
                [&](const auto&) {
                    if (mode == "selection") {
                        registry.Shutdown();
                    }
                    return true;
                }) == MC_OK,
        "fatal fixture initializes if no callback failed yet");
    if (mode == "shutdown" || mode == "initialize" || mode == "nested") {
        const auto view = registry.Find<mc_output_factory>(registry.ListPlugins().front().id);
        view.Invoke([&](const mc_output_factory&) {
            if (mode == "nested") {
                PluginRegistry nested;
                require(nested.Initialize(candidates.Directory()) == MC_OK, "nested independent session opens");
                const auto other = nested.Find<mc_output_factory>(nested.ListPlugins().front().id);
                return other.Invoke([&](const mc_output_factory&) {
                    return registry.Shutdown();
                });
            }
            return mode == "shutdown" ? registry.Shutdown() : registry.Initialize(candidates.Directory());
        });
    }
    return ReentryReturned;
}

/* negotiate, instantiate, and unload the actual production shared library.
the host compiles none of the sink implementation and imports no plugin C++ types. */
int main(int argc, char** argv) {
    candidateRoot = musik::core::sdk::fs::pathFromUtf8(argv[0]).parent_path() / "plugin-runs";
    if (argc == 3 && std::string(argv[1]).find("--fatal-") == 0) {
        return checkFatalReentry(std::string(argv[1]).substr(sizeof("--fatal-") - 1), argv[2]);
    }
    require(argc == 2, "expected the production nullout DLL path");
    checkRegistryOwnership(argv[1]);
    for (unsigned run = 0; run < 2; ++run) {
        PluginCandidates candidates(candidateRoot / "output-runs");
        const auto path = candidates.Add(musik::core::sdk::fs::pathFromUtf8(argv[1]), "nullout");
        PluginRegistry registry;
        require(!registry.Find<mc_output_factory>("absent"), "uninitialized registry has no output");
        require(registry.Initialize(candidates.Directory()) == MC_OK, "C-only output DLL loads");
        const auto catalog = registry.ListPlugins();
        require(catalog.size() == 1 && catalog.front().name == "Null Output", "DLL metadata is visible");
        auto capability = registry.Find<mc_output_factory>(catalog.front().id);
        require(bool(capability), "DLL registers its output factory");
        require(capability.Invoke([](const mc_output_factory& factory) {
            require(factory.instance_api && factory.instance_api->struct_size == sizeof(mc_output_vtable),
                "instance methods are part of the C contract");
            mc_output_options contractOptions = MC_OUTPUT_OPTIONS_DEFAULT;
            contractOptions.format = { sizeof(mc_audio_format), 1000, 1, 0 };
            output_contract::Check(factory, nullptr, contractOptions);
            checkPacing(factory);
            checkAdmission(factory);
            checkStop(factory);
            checkCleanup(factory);
            return MC_OK;
        }) == MC_OK,
            "all C output calls complete during the live registry session");
        /* another registry has no dependency on this active stack frame. */
        require(capability.Invoke([&](const mc_output_factory&) {
            PluginRegistry empty;
            auto completed = std::make_unique<PluginRegistry>();
            require(completed->Shutdown() == MC_OK, "completed session is empty");
            completed.reset();
            PluginRegistry independent;
            require(independent.Initialize(candidates.Directory()) == MC_OK,
                "independent session may initialize inside a plugin call");
            return independent.Shutdown();
        }) == MC_OK &&
                mapped(path),
            "independent shutdown leaves the calling session loaded");
        /* an error in the host operation must not be mislabeled as plugin failure. */
        bool hostFailure = false;
        try {
            capability.Invoke([](const mc_output_factory&) -> mc_result {
                throw std::logic_error("host operation failure");
            });
        }
        catch (const std::logic_error&) {
            hostFailure = true;
        }
        require(hostFailure, "host failure propagates; final shutdown checks restored state");
        auto copied = capability;
        require(copied.Invoke([&](const mc_output_factory& factory) {
            copied = {};
            return factory.read_default_device(factory.context, [](void*, const mc_output_device*) {}, nullptr);
        }) == MC_OK,
            "typed call survives resetting its original view");
        require(registry.Shutdown() == MC_OK && !mapped(path), "worker and instances stop before native unload");
        require(!registry.Find<mc_output_factory>(catalog.front().id), "closed session has no output lookup");
        capability = {};
    }
}
