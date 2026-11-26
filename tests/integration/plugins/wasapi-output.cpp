#include <musikcore/plugin.h>
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/services.h>
#include "../../support/plugins/OutputContract.h"
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    const mc_output_factory* factory = nullptr;
    const mc_configuration_api* configuration = nullptr;
    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "WASAPI output: %s\n", message);
            std::abort();
        }
    }
    mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
        const std::string_view id(capability->id.data, capability->id.length);
        if (id == MC_PLUGIN_CAPABILITY_OUTPUT) {
            factory = static_cast<const mc_output_factory*>(capability->api);
        }
        else if (id == MC_PLUGIN_CAPABILITY_CONFIGURATION) {
            configuration = static_cast<const mc_configuration_api*>(capability->api);
        }
        return MC_OK;
    }
    void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
    mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
    void MC_CALL destroyError(void*, mc_error**) {}
    mc_result MC_CALL readBoolean(void* context, mc_string_view key, bool fallback, bool* value) {
        const std::string_view name(key.data, key.length);
        *value = name == "allow_decoder_resampling" ? *static_cast<bool*>(context)
            : name == "enable_audio_endpoint_routing" ? true
            : fallback;
        return MC_OK;
    }
    struct Completions {
        std::atomic<unsigned> count { 0 };
        std::atomic<unsigned> readyCount { 0 };
        std::atomic<mc_result> result { MC_ERROR_INTERNAL };
    };
    void MC_CALL completed(void* context, uint64_t token, mc_result result) {
        auto& completion = *static_cast<Completions*>(context);
        require(token == 1, "token is preserved");
        completion.result.store(result);
        ++completion.count;
    }
    void MC_CALL ready(void* context) { ++static_cast<Completions*>(context)->readyCount; }
}

int main(int argc, char** argv) {
    require(argc == 2, "plugin path supplied");
    const auto library = LoadLibraryA(argv[1]);
    require(library != nullptr, "load plugin");
    const auto entry = reinterpret_cast<mc_plugin_get_api_fn>(GetProcAddress(library, "mc_plugin_get_api"));
    require(entry != nullptr, "C entry point exists");
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = logMessage;
    host.create_error = createError;
    host.destroy_error = destroyError;
    host.register_plugin_capability = registerCapability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate plugin");
    void* pluginState = nullptr;
    require(plugin.initialize(&host, &pluginState, nullptr) == MC_OK && factory && configuration,
        "register output and configuration");
    require(configuration->boolean_count == 2 && configuration->number_count == 0,
        "event-driven output has no device buffer-length setting");
    bool resample = false;
    mc_plugin_preferences_service preferences = MC_PLUGIN_PREFERENCES_SERVICE_DEFAULT;
    preferences.context = &resample;
    preferences.read_boolean = readBoolean;
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.preferences = &preferences;
    void* instance = nullptr;
    require(factory->create(factory->context, &services, &instance, nullptr) == MC_OK && instance,
        "create unopened instance");
    const auto* api = factory->instance_api;
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    require(api->get_default_format(instance, {}, &format) == MC_ERROR_UNSUPPORTED,
        "disabled decoder resampling leaves the decoder rate unchanged");
    resample = true;
    const auto formatResult = api->get_default_format(instance, {}, &format);
    require(formatResult == MC_OK || formatResult == MC_ERROR_NOT_FOUND, "query the default endpoint");
    if (formatResult == MC_OK) {
        require(format.sample_rate && format.channels, "mix format contains rate and channels");
        Completions completion;
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
        callbacks.context = &completion;
        callbacks.completed = completed;
        callbacks.ready = ready;
        mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
        options.format = format;
        options.queue_capacity_frames = 128;
        output_contract::Check(*factory, &services, options);
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare shared endpoint");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        mc_output_timing idle = MC_OUTPUT_TIMING_DEFAULT;
        require(api->get_timing(instance, &idle) == MC_OK &&
            (!(idle.valid_flags & MC_OUTPUT_TIMING_PLAYED) || idle.frames_played_by_driver == 0),
            "idle stream time is not reported as played PCM");
        std::vector<float> silence(256 * format.channels);
        mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
        buffer.token = 1;
        buffer.pcm = { sizeof(mc_pcm_view), silence.data(), 256, format };
        require(api->queue_audio_buffer(instance, &buffer) == MC_OK && completion.count.load() == 1 &&
            completion.result.load() == MC_OK, "return the accepted buffer exactly once");
        require(api->pause(instance) == MC_OK && api->resume(instance) == MC_OK, "pause and resume");
        require(api->drain(instance, nullptr) == MC_OK, "drain device padding");
        require(api->pause(instance) == MC_OK, "pause before queueing");
        buffer.pcm.frame_count = 256;
        require(api->queue_audio_buffer(instance, &buffer) == MC_ERROR_BUFFER_TOO_SMALL,
            "paused block exceeding capacity can never fit");
        completion.count.store(0);
        buffer.pcm.frame_count = 128;
        require(api->queue_audio_buffer(instance, &buffer) == MC_OK && completion.count.load() == 1,
            "paused device accepts PCM when it has space");
        buffer.pcm.frame_count = 1;
        require(api->queue_audio_buffer(instance, &buffer) == MC_ERROR_QUEUE_FULL,
            "paused full queue rejects a block that otherwise fits");
        require(api->drain(instance, nullptr) == MC_ERROR_IN_FLIGHT, "paused drain reports pending PCM");
        const auto readyBeforeResume = completion.readyCount.load();
        require(api->resume(instance) == MC_OK && api->drain(instance, nullptr) == MC_OK,
            "resumed device drains the paused PCM");
        require(completion.readyCount.load() > readyBeforeResume, "resume wakes a rejected producer");
        completion.count.store(0);
        const size_t pacedFrames = format.sample_rate / 4;
        std::vector<float> pacedSilence(pacedFrames * format.channels);
        buffer.pcm = { sizeof(mc_pcm_view), pacedSilence.data(), pacedFrames, format };
        require(api->queue_audio_buffer(instance, &buffer) == MC_OK && completion.count.load() == 1 &&
            completion.result.load() == MC_OK, "device events make progress on a paced write");
        require(api->drain(instance, nullptr) == MC_OK, "drain the paced write");
        completion.count.store(0);
        buffer.pcm.frame_count = 128;
        require(api->queue_audio_buffer(instance, &buffer) == MC_OK, "submit final short block");
        mc_output_timing beforeIdle = MC_OUTPUT_TIMING_DEFAULT;
        require(api->get_timing(instance, &beforeIdle) == MC_OK, "read timing after short block");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        mc_output_timing afterIdle = MC_OUTPUT_TIMING_DEFAULT;
        require(api->get_timing(instance, &afterIdle) == MC_OK, "read timing after playback becomes idle");
        if (beforeIdle.valid_flags & MC_OUTPUT_TIMING_PLAYED) {
            require(afterIdle.frames_played_by_driver == 256 + 128 + pacedFrames + 128 &&
                (afterIdle.valid_flags & MC_OUTPUT_TIMING_ESTIMATED),
                "timing advances to submitted frames without counting idle time");
        }
        mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
        require(api->get_timing(instance, &timing) == MC_OK, "read nonblocking timing snapshot");
        if (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) {
            require(timing.generation && timing.frames_played_timestamp_ns, "device clock has a generation and timestamp");
        }
        const auto generation = timing.generation;
        require(api->stop(instance) == MC_OK, "stop the first device generation");
        timing = MC_OUTPUT_TIMING_DEFAULT;
        require(api->get_timing(instance, &timing) == MC_OK && timing.valid_flags == 0,
            "stopped output does not report a stale device clock");
        require(api->prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare after stop");
        timing = MC_OUTPUT_TIMING_DEFAULT;
        require(api->get_timing(instance, &timing) == MC_OK, "query the new device generation");
        if (generation && (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED)) {
            require(timing.generation != generation, "reprepare changes the timing generation");
        }

        completion.count.store(0);
        const size_t longFrames = static_cast<size_t>(format.sample_rate) * 8;
        std::vector<float> longSilence(longFrames * format.channels);
        buffer.pcm = { sizeof(mc_pcm_view), longSilence.data(), longFrames, format };
        std::atomic<mc_result> writeResult { MC_ERROR_IN_FLIGHT };
        std::atomic<bool> writeStarted { false };
        std::thread writer([&] {
            writeStarted.store(true);
            writeResult.store(api->queue_audio_buffer(instance, &buffer));
        });
        const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!writeStarted.load() && std::chrono::steady_clock::now() < startDeadline) {
            std::this_thread::yield();
        }
        require(writeStarted.load(), "writer starts before stop");
        if (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) {
            for (int i = 0; i < 10000; ++i) {
                mc_output_timing concurrent = MC_OUTPUT_TIMING_DEFAULT;
                require(api->get_timing(instance, &concurrent) == MC_OK &&
                    (concurrent.valid_flags & MC_OUTPUT_TIMING_PLAYED) &&
                    concurrent.generation == timing.generation && concurrent.frames_played_timestamp_ns,
                    "concurrent reads retain a coherent last clock snapshot");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        require(writeResult.load() == MC_ERROR_IN_FLIGHT, "large write waits for device space");
        const auto started = std::chrono::steady_clock::now();
        require(api->stop(instance) == MC_OK, "stop interrupts a paced write");
        writer.join();
        require(std::chrono::steady_clock::now() - started < std::chrono::seconds(2), "stop is bounded");
        require((writeResult.load() == MC_OK && completion.count.load() == 1 &&
                    completion.result.load() == MC_ERROR_CANCELLED) ||
                (writeResult.load() == MC_ERROR_CANCELLED && completion.count.load() == 0),
            "interrupted PCM is either unaccepted or returned once as cancelled");
    }
    else {
        std::puts("No default WASAPI endpoint; playback checks skipped.");
    }
    require(api->stop(instance) == MC_OK, "stop before destroy");
    api->destroy(instance);
    plugin.shutdown(pluginState);
    require(FreeLibrary(library) != 0, "unload plugin");
}
