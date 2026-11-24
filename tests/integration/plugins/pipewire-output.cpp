#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include <dlfcn.h>
#include <chrono>
#include <atomic>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "PipeWire output: %s\n", message);
        std::abort();
    }
}

static const mc_output_factory* factory = nullptr;
static mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
    if (capability->id.length == std::strlen(MC_PLUGIN_CAPABILITY_OUTPUT) &&
        std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_OUTPUT, capability->id.length) == 0)
    {
        factory = static_cast<const mc_output_factory*>(capability->api);
    }
    return MC_OK;
}
static void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
static mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
static void MC_CALL destroyError(void*, mc_error**) {}
struct Completions {
    std::atomic<unsigned> count { 0 };
    std::atomic<uint64_t> token { 0 };
    std::atomic<mc_result> status { MC_ERROR_INTERNAL };
    int event = -1;
};
static void MC_CALL completed(void* context, uint64_t token, mc_result status) {
    auto& result = *static_cast<Completions*>(context);
    result.token.store(token);
    result.status.store(status);
    result.count.fetch_add(1);
    const uint64_t notification = 1;
    (void) write(result.event, &notification, sizeof(notification));
}
static void MC_CALL ready(void*) {}

int main(int argc, char** argv) {
    require(argc == 2, "plugin path supplied");
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
    void* instance = nullptr;
    require(factory->create(factory->context, nullptr, &instance, nullptr) == MC_OK && instance, "create output");
    const auto* api = factory->instance_api;
    Completions results;
    results.event = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    require(results.event >= 0, "create completion event");
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    callbacks.context = &results;
    callbacks.completed = completed;
    callbacks.ready = ready;
    mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
    options.format = { sizeof(mc_audio_format), 48000, 2, 0 };
    const char* device = std::getenv("MUSIKCUBE_TEST_PIPEWIRE_DEVICE");
    if (device) {
        options.device_id = { device, std::strlen(device) };
    }
    options.queue_capacity_frames = 4096;
    require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare PipeWire sink");
    float samples[8192] {};
    mc_output_buffer block = MC_OUTPUT_BUFFER_DEFAULT;
    block.token = 7;
    block.pcm = { sizeof(mc_pcm_view), samples, 4096, options.format };
    require(api->queue_audio_buffer(instance, &block) == MC_OK, "write PCM");
    pollfd notification { .fd = results.event, .events = POLLIN, .revents = 0 };
    require(poll(&notification, 1, 5000) == 1, "PCM returned before deadline");
    uint64_t count = 0;
    require(read(results.event, &count, sizeof(count)) == sizeof(count) && count == 1 &&
        results.count.load() == 1 && results.token.load() == 7 && results.status.load() == MC_OK,
        "return PCM once");
    require(api->drain(instance, nullptr) == MC_OK, "drain output");
    require(api->pause(instance) == MC_OK, "pause output");
    block.token = 8;
    require(api->queue_audio_buffer(instance, &block) == MC_OK && results.count.load() == 1,
        "paused output retains PCM");
    require(api->drain(instance, nullptr) == MC_ERROR_IN_FLIGHT, "paused drain does not wait");
    require(api->stop(instance) == MC_OK && api->stop(instance) == MC_OK, "stop is idempotent");
    require(results.count.load() == 2 && results.token.load() == 8 && results.status.load() == MC_ERROR_CANCELLED,
        "stop returns paused PCM once");
    require(api->queue_audio_buffer(instance, &block) == MC_ERROR_CANCELLED, "stopped output rejects audio");
    api->destroy(instance);
    plugin.shutdown(state);
    dlclose(library);
    close(results.event);
}
