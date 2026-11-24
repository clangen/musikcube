#pragma once

#include <musikcore/plugin/output.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace output_contract {
    inline void Require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "output contract: %s\n", message);
            std::abort();
        }
    }

    struct Completion {
        std::atomic<uint64_t> token { 0 };
        std::atomic<mc_result> result { MC_ERROR_INTERNAL };
        std::atomic<unsigned> count { 0 };
    };

    inline void MC_CALL Completed(void* context, uint64_t token, mc_result result) {
        auto& completion = *static_cast<Completion*>(context);
        completion.token.store(token, std::memory_order_relaxed);
        completion.result.store(result, std::memory_order_relaxed);
        completion.count.fetch_add(1, std::memory_order_release);
    }

    inline void MC_CALL Ready(void*) {}

    /* the caller chooses a working device and format. each accepted block may
    complete inline or later; the contract does not require a specific thread. */
    inline void Check(const mc_output_factory& factory, const mc_plugin_services* services,
        const mc_output_options& options, bool supportsPause = true)
    {
        void* instance = nullptr;
        Require(factory.create(factory.context, services, &instance, nullptr) == MC_OK && instance,
            "create unopened instance");
        const auto& api = *factory.instance_api;
        Completion completion;
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
        callbacks.context = &completion;
        callbacks.completed = Completed;
        callbacks.ready = Ready;
        Require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare output");
        Require(api.set_volume(instance, 0.5) == MC_OK, "set volume");
        double volume = 0;
        Require(api.get_volume(instance, &volume) == MC_OK && volume == 0.5, "read volume");
        if (supportsPause) {
            Require(api.pause(instance) == MC_OK && api.resume(instance) == MC_OK, "pause and resume");
        }

        std::vector<float> samples(options.format.channels, 0.0f);
        mc_output_buffer buffer = MC_OUTPUT_BUFFER_DEFAULT;
        buffer.token = 1;
        buffer.pcm = { sizeof(mc_pcm_view), samples.data(), 1, options.format };
        Require(api.queue_audio_buffer(instance, &buffer) == MC_OK, "accept PCM");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (completion.count.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(completion.count.load(std::memory_order_acquire) == 1 && completion.token.load() == 1 &&
                completion.result.load() == MC_OK,
            "return the accepted token once");
        Require(api.drain(instance, nullptr) == MC_OK, "drain accepted PCM");
        mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
        Require(api.get_timing(instance, &timing) == MC_OK, "read timing");
        if (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) {
            Require(timing.generation && timing.frames_played_timestamp_ns, "played timing has identity and timestamp");
        }
        if (api.struct_size >= offsetof(mc_output_vtable, flush) + sizeof(api.flush) && api.flush) {
            const auto flushed = api.flush(instance);
            Require(flushed == MC_OK || flushed == MC_ERROR_UNSUPPORTED, "flush or report unsupported");
            if (flushed == MC_OK) {
                buffer.token = 2;
                Require(api.queue_audio_buffer(instance, &buffer) == MC_OK, "accept PCM after flush");
                const auto afterFlushDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (completion.count.load(std::memory_order_acquire) < 2 &&
                    std::chrono::steady_clock::now() < afterFlushDeadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                Require(completion.count.load(std::memory_order_acquire) == 2 && completion.token.load() == 2 &&
                        completion.result.load() == MC_OK,
                    "return PCM accepted after flush");
                Require(api.drain(instance, nullptr) == MC_OK, "drain PCM after flush");
            }
        }
        const auto completedBeforeStop = completion.count.load(std::memory_order_acquire);
        Require(api.stop(instance) == MC_OK, "stop output");
        Require(completion.count.load(std::memory_order_acquire) == completedBeforeStop,
            "stop does not duplicate completion");
        Require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare again after stop");
        Require(api.stop(instance) == MC_OK, "stop the reused output");
        api.destroy(instance);
    }
}
