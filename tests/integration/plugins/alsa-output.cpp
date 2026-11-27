#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/settings.h>
#include <musikcore/plugin/services.h>
#include "../../support/plugins/OutputContract.h"
#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <poll.h>
#include <string_view>
#include <unistd.h>

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "ALSA output: %s\n", message);
        std::abort();
    }
}

static const mc_output_factory* factory = nullptr;
static const mc_configuration_api* configuration = nullptr;
static mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
    if (capability->id.length == std::strlen(MC_PLUGIN_CAPABILITY_CONFIGURATION) &&
        std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_CONFIGURATION, capability->id.length) == 0)
    {
        configuration = static_cast<const mc_configuration_api*>(capability->api);
        return MC_OK;
    }
    require(capability->id.length == std::strlen(MC_PLUGIN_CAPABILITY_OUTPUT) &&
        std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_OUTPUT, capability->id.length) == 0,
        "output capability");
    factory = static_cast<const mc_output_factory*>(capability->api);
    return MC_OK;
}
static void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
static mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
static void MC_CALL destroyError(void*, mc_error**) {}
struct Completions { unsigned count = 0; uint64_t token = 0; mc_result status = MC_ERROR_INTERNAL; };
static void MC_CALL completed(void* context, uint64_t token, mc_result status) {
    auto& result = *static_cast<Completions*>(context);
    ++result.count;
    result.token = token;
    result.status = status;
}
static void MC_CALL ready(void*) {}

static mc_result MC_CALL readLatency(void* context, const mc_integer_setting* setting, int32_t* value) {
    require(std::string_view(setting->key.data, setting->key.length) == "latency_ms", "latency setting key");
    const auto saved = *static_cast<int32_t*>(context);
    *value = saved >= setting->minimum && saved <= setting->maximum ? saved : setting->default_value;
    return MC_OK;
}

int main(int argc, char** argv) {
    require(argc == 2 || argc == 3, "plugin path supplied");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    require(library != nullptr, "plugin loads");
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(library, "mc_plugin_get_api"));
    require(entry != nullptr, "C plugin entry exists");
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = logMessage;
    host.create_error = createError;
    host.destroy_error = destroyError;
    host.register_plugin_capability = registerCapability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate plugin");
    void* state = nullptr;
    require(plugin.initialize(&host, &state, nullptr) == MC_OK && factory, "register output");
    require(configuration && configuration->integer_count == 1, "register latency configuration");
    const auto& latency = configuration->integers[0];
    require(std::string_view(latency.key.data, latency.key.length) == "latency_ms" &&
        latency.default_value == 500 && latency.minimum == 10 && latency.maximum == 5000,
        "latency setting describes milliseconds, default, and bounds");
    void* instance = nullptr;
    require(factory->create(factory->context, nullptr, &instance, nullptr) == MC_OK && instance, "create output");
    const auto* api = factory->instance_api;
    Completions results;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.context = &results;
    callbacks.completed = completed;
    callbacks.ready = ready;
    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format = { sizeof(mc_audio_format), 48000, 2, 0 };
    options.device_id = { "null", 4 };
    options.queue_capacity_frames = 512;
    options.initial_volume = 0.5;
    require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare ALSA null device");
    float samples[512] {};
    std::fill(std::begin(samples), std::end(samples), 0.8f);
    auto lastSample = reinterpret_cast<float (*)()>(dlsym(RTLD_DEFAULT, "mc_test_alsa_last_sample"));
    if (argc == 3) {
        require(lastSample != nullptr, "sample capture fixture loaded");
    }
    mc_output_buffer block = MC_OUTPUT_BUFFER_DEFAULT;
    block.token = 7;
    block.pcm = { sizeof(mc_pcm_view), samples, 256, options.format };
    require(api->queue_audio_buffer(instance, &block) == MC_OK, "write PCM");
    require(results.count == 1 && results.token == 7 && results.status == MC_OK, "return PCM once");
    if (lastSample) {
        require(std::abs(lastSample() - 0.1f) < 0.0001f, "half volume applies a cubic gain");
        require(samples[0] == 0.8f, "volume leaves borrowed samples unchanged");
    }
    double reportedVolume = -1;
    require(api->get_volume(instance, &reportedVolume) == MC_OK && reportedVolume == 0.5,
        "reported volume remains the requested level");
    require(api->drain(instance, nullptr) == MC_OK, "drain output");
    require(api->set_volume(instance, 1.0) == MC_OK, "set full volume");
    block.token = 8;
    require(api->queue_audio_buffer(instance, &block) == MC_OK && results.count == 2 &&
        results.token == 8 && results.status == MC_OK, "write again after draining");
    if (lastSample) {
        require(std::abs(lastSample() - 0.8f) < 0.0001f, "full volume preserves the samples");
    }
    require(api->set_volume(instance, 0.0) == MC_OK, "mute output");
    block.token = 9;
    require(api->queue_audio_buffer(instance, &block) == MC_OK && results.count == 3,
        "write muted PCM");
    if (lastSample) {
        require(lastSample() == 0.0f, "zero volume silences the samples");
    }
    block.pcm.frame_count = 513;
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_BUFFER_TOO_SMALL && results.count == 3,
        "oversized PCM remains with the host");
    require(api->stop(instance) == MC_OK && api->stop(instance) == MC_OK, "stop is idempotent");
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_CANCELLED, "stopped output rejects audio");
    int32_t savedLatency = 125;
    mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT;
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    /* the shim makes ALSA return EAGAIN and holds its poll descriptor quiet.
    stop must wake the blocked producer and return its accepted token. */
    if (argc == 3) {
        auto requestedLatency = reinterpret_cast<unsigned (*)()>(dlsym(RTLD_DEFAULT, "mc_test_alsa_latency"));
        require(requestedLatency && requestedLatency() == 500000, "default latency reaches ALSA in microseconds");
        api->destroy(instance);
        settings.context = &savedLatency;
        settings.read_integer = readLatency;
        services.settings = &settings;
        require(factory->create(factory->context, &services, &instance, nullptr) == MC_OK,
            "create output with settings");
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK && requestedLatency() == 125000,
            "configured latency reaches ALSA in microseconds");
        require(api->stop(instance) == MC_OK, "stop configured output");
        savedLatency = 5001;
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK && requestedLatency() == 500000,
            "out-of-range saved latency uses default");
        require(api->stop(instance) == MC_OK, "stop defaulted output");
        options.device_id = { "unavailable", 11 };
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK,
            "unavailable pinned device falls back to default");
        require(api->stop(instance) == MC_OK, "stop fallback device");
        options.device_id = { "null", 4 };
        auto stall = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "mc_test_alsa_stall"));
        auto stallDrain = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "mc_test_alsa_stall_drain"));
        auto unstall = reinterpret_cast<void (*)()>(dlsym(RTLD_DEFAULT, "mc_test_alsa_unstall"));
        require(stall && stallDrain && unstall, "stall fixture loaded");
        int gate[2];
        require(pipe(gate) == 0, "create write gate");
        stall(gate[1]);
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare stalled output");
        block.token = 9;
        block.pcm.frame_count = 256;
        auto writer = std::async(std::launch::async, [&] { return api->queue_audio_buffer(instance, &block); });
        pollfd pending { .fd = gate[0], .events = POLLIN, .revents = 0 };
        require(poll(&pending, 1, 5000) == 1, "producer reached stalled write");
        auto stop = std::async(std::launch::async, [&] { return api->stop(instance); });
        require(stop.wait_for(std::chrono::seconds(5)) == std::future_status::ready && stop.get() == MC_OK,
            "stop interrupts the stalled ALSA device");
        require(writer.wait_for(std::chrono::seconds(5)) == std::future_status::ready && writer.get() == MC_OK,
            "accepted stalled buffer settles");
        require(results.count == 4 && results.token == 9 && results.status == MC_ERROR_CANCELLED,
            "stalled buffer returned exactly once as cancelled");
        unstall();
        char notification;
        require(read(gate[0], &notification, 1) == 1 && notification == 'w', "consume write notification");
        stallDrain(gate[1]);
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare stalled drain");
        auto drain = std::async(std::launch::async, [&] { return api->drain(instance, nullptr); });
        pending.revents = 0;
        require(poll(&pending, 1, 5000) == 1, "drain reached stalled device");
        auto stopDrain = std::async(std::launch::async, [&] { return api->stop(instance); });
        require(stopDrain.wait_for(std::chrono::seconds(5)) == std::future_status::ready && stopDrain.get() == MC_OK,
            "stop interrupts stalled drain");
        require(drain.wait_for(std::chrono::seconds(5)) == std::future_status::ready &&
            drain.get() == MC_ERROR_CANCELLED, "stalled drain reports cancellation");
        require(results.count == 4, "drain cancellation does not duplicate completion");
        unstall();
        close(gate[0]);
        close(gate[1]);
    }
    api->destroy(instance);
    /* ALSA's null PCM does not implement hardware pause. */
    output_contract::Check(*factory, nullptr, options, false);
    plugin.shutdown(state);
    dlclose(library);
}
