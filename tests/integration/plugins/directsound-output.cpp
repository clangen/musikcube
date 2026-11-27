#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include "../../support/plugins/OutputContract.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    const mc_output_factory* factory = nullptr;
    void require(bool value, const char* message) {
        if (!value) { std::fprintf(stderr, "DirectSound: %s\n", message); std::abort(); }
    }
    mc_result MC_CALL Register(void*, const mc_plugin_capability* capability) {
        if (std::string_view(capability->id.data, capability->id.length) == MC_PLUGIN_CAPABILITY_OUTPUT)
            factory = static_cast<const mc_output_factory*>(capability->api);
        return MC_OK;
    }
    void MC_CALL Log(void*, mc_log_level, mc_string_view, mc_string_view) {}
    mc_result MC_CALL Error(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
    void MC_CALL DestroyError(void*, mc_error**) {}
    struct Callbacks { std::atomic<unsigned> completed { 0 }; std::atomic<unsigned> ready { 0 }; };
    void MC_CALL Complete(void* context, uint64_t token, mc_result result) {
        require(token == 1 && result == MC_OK, "completion token and result");
        ++static_cast<Callbacks*>(context)->completed;
    }
    void MC_CALL Ready(void* context) { ++static_cast<Callbacks*>(context)->ready; }
    void MC_CALL Found(void* context, const mc_output_device* device) {
        require(device->id.length && device->name.length, "device has id and name");
        *static_cast<bool*>(context) = true;
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "plugin path");
    HMODULE library = LoadLibraryA(argv[1]);
    require(library != nullptr, "load plugin");
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(GetProcAddress(library, "mc_plugin_get_api"));
    require(entry != nullptr, "C plugin entry");
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = Log; host.create_error = Error; host.destroy_error = DestroyError; host.register_plugin_capability = Register;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate plugin");
    void* pluginState = nullptr;
    require(plugin.initialize(&host, &pluginState, nullptr) == MC_OK && factory, "register output");
    bool found = false;
    const auto defaultResult = factory->read_default_device(factory->context, Found, &found);
    require(defaultResult == MC_OK || defaultResult == MC_ERROR_NOT_FOUND, "query default device");
    if (defaultResult == MC_OK) {
        require(found, "default device reported");
        mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
        options.format = { sizeof(mc_audio_format), 48000, 2, 0 };
        options.queue_capacity_frames = 2048;
        output_contract::Check(*factory, nullptr, options);
        void* instance = nullptr;
        require(factory->create(factory->context, nullptr, &instance, nullptr) == MC_OK, "create");
        const auto& api = *factory->instance_api;
        Callbacks record;
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
        callbacks.context = &record; callbacks.completed = Complete; callbacks.ready = Ready;
        require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare");
        require(api.pause(instance) == MC_OK, "pause");
        std::vector<float> data(4096, 0.0f);
        mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
        buffer.token = 1; buffer.pcm = { sizeof(mc_pcm_view), data.data(), 2048, options.format };
        require(api.queue_audio_buffer(instance, &buffer) == MC_OK && record.completed == 1, "queue while paused");
        buffer.pcm.frame_count = 1;
        require(api.queue_audio_buffer(instance, &buffer) == MC_ERROR_QUEUE_FULL, "paused queue full");
        require(api.drain(instance, nullptr) == MC_ERROR_IN_FLIGHT, "paused drain");
        require(api.resume(instance) == MC_OK, "resume");
        require(api.drain(instance, nullptr) == MC_OK, "drain");
        mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
        require(api.get_timing(instance, &timing) == MC_OK && (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) &&
            timing.frames_played_by_driver == 2048, "played timing excludes silence");
        require(record.ready > 0, "ready notified");
        require(api.pause(instance) == MC_OK, "pause before filling block slots");
        for (int i = 0; i < 256; ++i) {
            require(api.queue_audio_buffer(instance, &buffer) == MC_OK, "fill a pending block slot");
        }
        require(api.queue_audio_buffer(instance, &buffer) == MC_ERROR_QUEUE_FULL,
            "bounded block queue reports temporary backpressure");
        const auto readyBeforeResume = record.ready.load();
        require(api.resume(instance) == MC_OK && api.drain(instance, nullptr) == MC_OK,
            "resume drains pending block slots");
        require(record.ready.load() > readyBeforeResume, "slot release wakes the producer");
        require(api.pause(instance) == MC_OK, "pause before replacing a track");
        require(api.flush && api.flush(instance) == MC_OK, "flush the prepared endpoint");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        require(api.queue_audio_buffer(instance, &buffer) == MC_OK, "submit replacement without preparing again");
        require(api.drain(instance, nullptr) == MC_OK, "drain replacement PCM");
        require(api.stop(instance) == MC_OK, "stop");

        options.queue_capacity_frames = 96000;
        require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare for interrupted drain");
        std::vector<float> longAudio(96000 * options.format.channels, 0.0f);
        buffer.pcm = { sizeof(mc_pcm_view), longAudio.data(), 96000, options.format };
        require(api.queue_audio_buffer(instance, &buffer) == MC_OK, "queue audio for interrupted drain");
        std::atomic<mc_result> drainResult { MC_ERROR_IN_FLIGHT };
        std::thread drainer([&] { drainResult = api.drain(instance, nullptr); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        require(drainResult == MC_ERROR_IN_FLIGHT, "drain waits for playback");
        require(api.stop(instance) == MC_OK, "stop interrupts drain");
        drainer.join();
        require(drainResult == MC_ERROR_CANCELLED, "interrupted drain reports cancellation");

        require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare for paused stop race");
        require(api.pause(instance) == MC_OK, "pause before stop race");
        buffer.pcm = { sizeof(mc_pcm_view), data.data(), 1, options.format };
        require(api.queue_audio_buffer(instance, &buffer) == MC_OK, "queue paused audio before stop race");
        std::atomic<bool> stopFinished { false };
        std::thread stopper([&] {
            require(api.stop(instance) == MC_OK, "stop paused output");
            stopFinished = true;
        });
        while (!stopFinished.load()) {
            const auto result = api.drain(instance, nullptr);
            require(result == MC_ERROR_IN_FLIGHT || result == MC_ERROR_CANCELLED, "paused drain during stop");
        }
        stopper.join();
        require(api.drain(instance, nullptr) == MC_ERROR_CANCELLED, "stopped drain reports cancellation");
        api.destroy(instance);
    } else std::puts("No default DirectSound endpoint; playback checks skipped.");
    plugin.shutdown(pluginState);
    require(FreeLibrary(library) != 0, "unload");
}
