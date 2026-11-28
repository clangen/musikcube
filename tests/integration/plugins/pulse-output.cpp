#include "../../support/plugins/OutputContract.h"
#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/services.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <poll.h>
#include <unistd.h>

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "PulseAudio output: %s\n", message);
        std::abort();
    }
}

static const mc_output_factory* factory = nullptr;
static mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
    require(capability->id.length == std::strlen(MC_PLUGIN_CAPABILITY_OUTPUT) &&
        std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_OUTPUT, capability->id.length) == 0,
        "output capability");
    factory = static_cast<const mc_output_factory*>(capability->api);
    return MC_OK;
}
static void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
static mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
static void MC_CALL destroyError(void*, mc_error**) {}
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
    mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT;
    settings.read_boolean = [](void*, mc_string_view key, bool, bool* value) -> mc_result {
        require(key.length == 19 && std::memcmp(key.data, "force_linear_volume", 19) == 0,
            "read PulseAudio setting");
        *value = true;
        return MC_OK;
    };
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.settings = &settings;
    void* instance = nullptr;
    require(factory->create(factory->context, &services, &instance, nullptr) == MC_OK && instance, "create output");
    const auto* api = factory->instance_api;
    output_contract::Completion results;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.context = &results;
    callbacks.completed = output_contract::Completed;
    callbacks.ready = output_contract::Ready;
    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format = { sizeof(mc_audio_format), 48000, 2, 0 };
    const char* device = std::getenv("MUSIKCUBE_TEST_PULSE_DEVICE");
    if (!device) {
        device = "null";
    }
    options.device_id = { device, std::strlen(device) };
    options.queue_capacity_frames = 512;
    options.initial_volume = 0.5;
    const auto forbidWait = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "mc_test_pulse_forbid_wait"));
    const auto requestedVolume =
        reinterpret_cast<unsigned (*)()>(dlsym(RTLD_DEFAULT, "mc_test_pulse_requested_volume"));
    const auto stallDrain = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "mc_test_pulse_stall_drain"));
    require(forbidWait && requestedVolume && stallDrain, "load native PulseAudio test probes");
    require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare PulseAudio device");
    require(requestedVolume() == 32768, "linear volume setting applies during prepare");

    float samples[512] {};
    mc_output_buffer block = MC_OUTPUT_BUFFER_DEFAULT;
    block.token = 7;
    block.pcm = { sizeof(mc_pcm_view), samples, 256, options.format };
    auto timing = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
    forbidWait(1);
    require(api->pause(instance) == MC_OK, "pause without waiting for acknowledgement");
    require(api->queue_audio_buffer(instance, &block) == MC_OK && results.count.load() == 0,
        "producer returns immediately and retains one paused block");
    block.token = 8;
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_QUEUE_FULL,
        "a full slot leaves the next block with the caller");
    require(api->get_timing(instance, &timing) == MC_OK &&
        (timing.valid_flags & MC_OUTPUT_TIMING_QUEUED) && timing.frames_played_by_driver == 0 &&
        timing.queued_seconds > 0, "paused PCM is pending");
    require(api->drain(instance, nullptr) == MC_ERROR_IN_FLIGHT, "paused drain does not wait");
    require(api->set_volume(instance, 0.75) == MC_OK, "set volume without waiting for acknowledgement");
    require(api->resume(instance) == MC_OK, "resume without waiting for acknowledgement");
    forbidWait(0);
    require(api->drain(instance, nullptr) == MC_OK, "drain waits for pending PCM and the server");
    require(results.count.load() == 1 && results.token.load() == 7 && results.result.load() == MC_OK,
        "return the accepted token once");
    require(api->get_timing(instance, &timing) == MC_OK &&
        (timing.valid_flags & MC_OUTPUT_TIMING_ESTIMATED) &&
        timing.frames_played_by_driver == block.pcm.frame_count && timing.queued_seconds == 0,
        "drain reaches the accepted PCM tail");
    const auto generation = timing.generation;
    require(api->pause(instance) == MC_OK && api->queue_audio_buffer(instance, &block) == MC_OK,
        "queue a paused block for flush");
    require(api->flush(instance) == MC_OK && results.count.load() == 2 && results.token.load() == 8 &&
        results.result.load() == MC_ERROR_CANCELLED, "flush cancels the unwritten token");
    require(api->get_timing(instance, &timing) == MC_OK && timing.generation != generation &&
        timing.frames_played_by_driver == 0 && timing.queued_seconds == 0, "flush resets the clock");
    block.pcm.frame_count = 513;
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_BUFFER_TOO_SMALL,
        "oversized PCM remains with the caller");
    block.pcm.frame_count = 256;
    block.token = 9;
    require(api->pause(instance) == MC_OK && api->queue_audio_buffer(instance, &block) == MC_OK,
        "queue a paused block for stop");
    require(api->stop(instance) == MC_OK && api->stop(instance) == MC_OK, "stop is idempotent");
    require(results.count.load() == 3 && results.token.load() == 9 && results.result.load() == MC_ERROR_CANCELLED,
        "stop cancels the unwritten token once");
    require(api->get_timing(instance, &timing) == MC_OK && timing.valid_flags == 0,
        "stop clears the timing snapshot");
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_CANCELLED, "stopped output rejects audio");

    options.device_id = { "unavailable", 11 };
    require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK,
        "unavailable pinned device falls back to default");
    int gate[2];
    require(pipe(gate) == 0, "create drain gate");
    stallDrain(gate[1]);
    auto worker = std::async(std::launch::async, [&] { return api->drain(instance, nullptr); });
    pollfd pending { .fd = gate[0], .events = POLLIN, .revents = 0 };
    require(poll(&pending, 1, 5000) == 1, "drain reached native operation gate");
    require(api->stop(instance) == MC_OK, "stop interrupts the native drain");
    require(worker.wait_for(std::chrono::seconds(5)) == std::future_status::ready &&
        worker.get() == MC_ERROR_CANCELLED, "interrupted drain returns cancellation");
    stallDrain(-1);
    close(gate[0]);
    close(gate[1]);
    options.device_id = { device, std::strlen(device) };
    api->destroy(instance);
    output_contract::Check(*factory, &services, options);
    plugin.shutdown(state);
    dlclose(library);
}
