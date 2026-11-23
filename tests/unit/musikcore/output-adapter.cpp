#include <musikcore/plugin/output.hpp>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

/* this test links no musikcore library and sees only public SDK headers.
registration captures real generated C tables, then calls them as a C host would. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "output adapter: %s\n", message);
        std::abort();
    }
}

/* controlled device implementation, with no adapter inheritance. counters
observe construction, partial prepare rollback, and destruction through C calls. */
struct TestOutput {
    static inline unsigned constructed = 0;
    static inline unsigned destroyed = 0;
    static inline unsigned stops = 0;
    static inline bool throwConstruct = false;
    static inline bool throwPrepare = false;
    bool running = false;
    double volume = 1;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;

    /* select the services-aware constructor without requiring a defined bundle. */
    explicit TestOutput(const mc_plugin_services*) {
        if (throwConstruct) {
            throw std::runtime_error("constructor failed");
        }
        ++constructed;
    }

    /* the owning host must stop the implementation before deleting it. */
    ~TestOutput() {
        require(!this->running, "destroy follows Stop");
        ++destroyed;
    }

    /* simulate partial device opening, including failure after state changes. */
    mc_result Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
        require(!this->running, "host stops old state before reprepare");
        this->running = true;
        if (throwPrepare) {
            throw std::runtime_error("device open failed");
        }
        this->volume = options.initial_volume;
        this->callbacks = callbacks;
        return MC_OK;
    }

    /* prove the trampoline forwards the original block and completion state. */
    mc_result QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
        this->callbacks.completed(this->callbacks.context, buffer.token, MC_OK);
        return MC_OK;
    }

    /* ordinary method exceptions must not escape a generated C function. */
    mc_result Pause() { throw std::runtime_error("pause failed"); }

    /* successful ordinary operations retain their result. */
    mc_result Resume() { return MC_OK; }

    /* normalized volume arrives as a validated host value. */
    mc_result SetVolume(double value) {
        this->volume = value;
        return MC_OK;
    }

    /* getters use a plain C++ value rather than an output-pointer convention. */
    double GetVolume() const { return this->volume; }

    /* cleanup is noexcept; returning failure is deliberate fatal-path injection. */
    mc_result Stop() noexcept {
        ++stops;
        this->running = false;
        return MC_OK;
    }

    /* synchronous processing leaves no backend queue. */
    mc_result Drain() { return MC_OK; }

    /* publish callback-scoped metadata through the generated factory. */
    static mc_result VisitDevices(mc_output_device_fn read, void* user) {
        const mc_output_device device { { "test", 4 }, { "Test output", 11 } };
        read(user, &device);
        return MC_OK;
    }

    /* the default device uses the same snapshot as enumeration. */
    static mc_result ReadDefaultDevice(mc_output_device_fn read, void* user) { return VisitDevices(read, user); }

    /* fill a reference, with no C ABI pointer or exception boilerplate. */
    static mc_result GetDefaultFormat(mc_string_view, mc_audio_format& format) {
        format = { sizeof(format), 48000, 2, 0 };
        return MC_OK;
    }
};

/* optional timing uses the same generated C table without a base class. */
struct MeasuredOutput : TestOutput {
    using TestOutput::TestOutput;

    /* report a bounded snapshot; the adapter supplies safe initial defaults. */
    mc_result GetTiming(mc_output_timing& timing) const noexcept {
        timing.valid_flags = MC_OUTPUT_TIMING_PLAYED;
        timing.generation = 3;
        timing.frames_played_by_driver = 42;
        timing.frames_played_timestamp_ns = 1234;
        return MC_OK;
    }
};

/* register while the host callback is active, then retain only the static table.
there is no core linkage, private header access, or adapter-owned runtime object. */
template<typename T> static const mc_output_factory& factoryFor() {
    const mc_output_factory* factory = nullptr;
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.host_context = &factory;
    host.register_capability = [](void* context, const mc_plugin_capability* capability) -> mc_result {
        require(std::string(capability->id.data, capability->id.length) == MC_PLUGIN_CAPABILITY_OUTPUT,
            "adapter supplies the output capability identifier");
        require(capability->api_size == sizeof(mc_output_factory), "adapter supplies the correct table size");
        *static_cast<const mc_output_factory**>(context) = static_cast<const mc_output_factory*>(capability->api);
        return MC_OK;
    };
    require(
        musik::core::plugin::RegisterOutput<T>(host) == MC_OK && factory, "registration publishes a static C factory");
    return *factory;
}

/* registration supplies static sized tables; the host has validated its own
bootstrap callbacks before initialize reaches this adapter. */
static void checkRegistration() {
    const auto& factory = factoryFor<TestOutput>();
    require(factory.instance_api && factory.instance_api->struct_size == sizeof(mc_output_vtable),
        "sized C instance table");
    unsigned devices = 0;
    require(factory.visit_devices(
                nullptr,
                [](void* user, const mc_output_device* device) {
                    ++*static_cast<unsigned*>(user);
                    require(device->id.length == 4, "device callback receives implementation metadata");
                },
                &devices) == MC_OK &&
            devices == 1,
        "static device methods are forwarded");
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    require(factory.get_default_format(nullptr, { nullptr, 0 }, &format) == MC_OK && format.sample_rate == 48000,
        "default format reference is forwarded");
}

/* the adapter forwards calls without injecting Stop or rollback. the owning
host explicitly stops before reuse, after failed prepare, and before destruction. */
static void checkInstances() {
    const auto& factory = factoryFor<TestOutput>();
    const auto& api = *factory.instance_api;
    void* instance = nullptr;
    TestOutput::throwConstruct = true;
    require(factory.create(nullptr, nullptr, &instance, nullptr) == MC_ERROR_INTERNAL && !instance,
        "constructor failure leaves the ownership slot empty");
    TestOutput::throwConstruct = false;
    require(factory.create(nullptr, nullptr, &instance, nullptr) == MC_OK && instance && TestOutput::constructed == 1,
        "successful construction transfers one implementation instance");
    unsigned completed = 0;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.context = &completed;
    callbacks.completed = [](void* context, uint64_t token, mc_result outcome) {
        require(token == 7 && outcome == MC_OK, "completion identity passes through unchanged");
        ++*static_cast<unsigned*>(context);
    };
    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format = { sizeof(mc_audio_format), 48000, 2, 0 };

    /* a failed device open may leave partial state. only the owning host decides
    when to stop it; exception containment must not hide lifecycle calls. */
    const auto beforeFailure = TestOutput::stops;
    TestOutput::throwPrepare = true;
    require(api.prepare(instance, &options, &callbacks, nullptr) == MC_ERROR_INTERNAL &&
            static_cast<TestOutput*>(instance)->running && TestOutput::stops == beforeFailure,
        "prepare translates exceptions without automatic cleanup");
    require(api.stop(instance) == MC_OK, "host cleans partial prepare state");
    TestOutput::throwPrepare = false;
    const auto beforePrepare = TestOutput::stops;
    require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK && TestOutput::stops == beforePrepare,
        "prepare does not insert another Stop");
    require(api.stop(instance) == MC_OK, "host stops before reprepare");
    require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "host reuses the stopped instance");

    require(api.set_volume(instance, 0.25) == MC_OK, "validated host volume forwarded");
    double volume = 0;
    require(api.get_volume(instance, &volume) == MC_OK && volume == 0.25, "plain getter value crosses the C table");
    require(api.pause(instance) == MC_ERROR_INTERNAL && api.resume(instance) == MC_OK,
        "ordinary implementation exceptions remain contained");
    float samples[2] {};
    mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
    buffer.token = 7;
    buffer.pcm = { sizeof(mc_pcm_view), samples, 1, options.format };
    require(api.queue_audio_buffer(instance, &buffer) == MC_OK && completed == 1, "validated PCM uses generated glue");
    mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
    timing.valid_flags = MC_OUTPUT_TIMING_PLAYED;
    require(api.get_timing(instance, &timing) == MC_OK && !timing.valid_flags, "absent timing reports unknown");
    require(api.drain(instance, nullptr) == MC_OK, "drain forwarded");
    require(api.stop(instance) == MC_OK, "host quiesces before destroy");
    const auto beforeDestroy = TestOutput::stops;
    api.destroy(instance);
    require(TestOutput::destroyed == 1 && TestOutput::stops == beforeDestroy,
        "destroy deletes once without an implicit Stop");
}

/* a timing method is optional, but when present its values cross the C table. */
static void checkTiming() {
    const auto& factory = factoryFor<MeasuredOutput>();
    void* instance = nullptr;
    require(factory.create(nullptr, nullptr, &instance, nullptr) == MC_OK, "timed instance creates");
    mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
    require(factory.instance_api->get_timing(instance, &timing) == MC_OK &&
            timing.valid_flags == MC_OUTPUT_TIMING_PLAYED && timing.generation == 3 &&
            timing.frames_played_by_driver == 42 && timing.frames_played_timestamp_ns == 1234,
        "optional timing method supplies its actual snapshot");
    require(factory.instance_api->stop(instance) == MC_OK, "host stops timed instance before destroy");
    factory.instance_api->destroy(instance);
}

/* public-header-only checks use a valid host sequence and no core linkage. */
int main() {
    checkRegistration();
    checkInstances();
    checkTiming();
    return 0;
}
