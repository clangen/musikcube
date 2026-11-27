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
struct Completions { unsigned count = 0; uint64_t token = 0; mc_result status = MC_ERROR_INTERNAL; };
static void MC_CALL completed(void* context, uint64_t token, mc_result status) {
    auto& result = *static_cast<Completions*>(context);
    ++result.count;
    result.token = token;
    result.status = status;
}
static void MC_CALL ready(void*) {}

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
    Completions results;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.context = &results;
    callbacks.completed = completed;
    callbacks.ready = ready;
    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format = { sizeof(mc_audio_format), 48000, 2, 0 };
    const char* device = std::getenv("MUSIKCUBE_TEST_PULSE_DEVICE");
    if (!device) {
        device = "null";
    }
    options.device_id = { device, std::strlen(device) };
    options.queue_capacity_frames = 512;
    options.initial_volume = 0.5;
    int volumeGate[2] { -1, -1 };
    if (argc == 3 && std::strcmp(argv[2], "fixture") == 0) {
        require(pipe(volumeGate) == 0, "create volume gate");
        const auto fd = std::to_string(volumeGate[1]);
        require(setenv("MUSIKCUBE_TEST_PULSE_VOLUME_FD", fd.c_str(), 1) == 0, "set volume gate");
    }
    require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare PulseAudio fixture device");
    if (argc == 3 && std::strcmp(argv[2], "fixture") == 0) {
        int volume = 0;
        require(read(volumeGate[0], &volume, sizeof(volume)) == sizeof(volume) && volume == 32768,
            "linear volume setting applies during prepare");
        unsetenv("MUSIKCUBE_TEST_PULSE_VOLUME_FD");
        close(volumeGate[0]);
        close(volumeGate[1]);
    }
    float samples[512] {};
    mc_output_buffer block = MC_OUTPUT_BUFFER_DEFAULT;
    block.token = 7;
    block.pcm = { sizeof(mc_pcm_view), samples, 256, options.format };
    require(api->queue_audio_buffer(instance, &block) == MC_OK, "write PCM");
    require(results.count == 1 && results.token == 7 && results.status == MC_OK, "return PCM once");
    require(api->drain(instance, nullptr) == MC_OK, "drain output");
    auto forbidWait = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "mc_test_pulse_forbid_wait"));
    if (argc == 3 && std::strcmp(argv[2], "no-wait") == 0) {
        require(forbidWait != nullptr, "PulseAudio wait fixture loaded");
        forbidWait(1);
    }
    require(api->pause(instance) == MC_OK, "pause output");
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_QUEUE_FULL && results.count == 1,
        "paused output preserves the host buffer");
    require(api->drain(instance, nullptr) == MC_ERROR_IN_FLIGHT, "paused drain does not wait");
    require(api->resume(instance) == MC_OK, "resume output");
    if (forbidWait) {
        require(api->set_volume(instance, 0.75) == MC_OK, "set volume without waiting for acknowledgement");
        forbidWait(0);
    }
    require(api->flush(instance) == MC_OK, "flush keeps output usable");
    block.token = 8;
    require(api->queue_audio_buffer(instance, &block) == MC_OK && results.count == 2 &&
        results.token == 8 && results.status == MC_OK, "write again after draining");
    block.pcm.frame_count = 513;
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_BUFFER_TOO_SMALL && results.count == 2,
        "oversized PCM remains with the host");
    require(api->stop(instance) == MC_OK && api->stop(instance) == MC_OK, "stop is idempotent");
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_CANCELLED, "stopped output rejects audio");
    if (argc == 3 && std::strcmp(argv[2], "fixture") == 0) {
        options.device_id = { "unavailable", 11 };
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK,
            "unavailable pinned device falls back to default");
        require(api->stop(instance) == MC_OK, "stop fallback device");
        options.device_id = { device, std::strlen(device) };
        int gate[2];
        require(pipe(gate) == 0, "create stall gate");
        const auto fd = std::to_string(gate[1]);
        require(setenv("MUSIKCUBE_TEST_PULSE_GATE_FD", fd.c_str(), 1) == 0, "set stall gate");
        for (const char* mode : { "write", "drain" }) {
            require(setenv("MUSIKCUBE_TEST_PULSE_STALL", mode, 1) == 0, "set stall mode");
            require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare stalled output");
            block.token = 9;
            block.pcm.frame_count = 256;
            const bool writing = std::strcmp(mode, "write") == 0;
            auto worker = std::async(std::launch::async, [&] {
                return writing ? api->queue_audio_buffer(instance, &block) : api->drain(instance, nullptr);
            });
            pollfd pending { .fd = gate[0], .events = POLLIN, .revents = 0 };
            require(poll(&pending, 1, 5000) == 1, "operation reached stall gate");
            char signal;
            require(read(gate[0], &signal, 1) == 1, "consume stall notification");
            auto stop = std::async(std::launch::async, [&] { return api->stop(instance); });
            require(stop.wait_for(std::chrono::seconds(5)) == std::future_status::ready && stop.get() == MC_OK,
                "stop interrupts stalled operation");
            require(worker.wait_for(std::chrono::seconds(5)) == std::future_status::ready &&
                worker.get() == (writing ? MC_OK : MC_ERROR_CANCELLED), "stalled operation settles");
            require(results.count == 3 && results.token == 9 && results.status == MC_ERROR_CANCELLED,
                "stalled write returns one cancelled token");
        }
        unsetenv("MUSIKCUBE_TEST_PULSE_STALL");
        unsetenv("MUSIKCUBE_TEST_PULSE_GATE_FD");
        close(gate[0]);
        close(gate[1]);
    }
    api->destroy(instance);
    output_contract::Check(*factory, &services, options);
    plugin.shutdown(state);
    dlclose(library);
}
