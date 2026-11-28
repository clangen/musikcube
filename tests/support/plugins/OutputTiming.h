#pragma once

#include "OutputContract.h"
#include <cmath>

namespace output_contract {

    /* exercise the actual C table and device clock, including admission while
    paused. completion only returns storage; timing must reach the audible tail. */
    inline void CheckTiming(const mc_output_factory& factory, mc_output_options options) {
        void* instance = nullptr;
        const auto& api = *factory.instance_api;
        Require(factory.create(factory.context, nullptr, &instance, nullptr) == MC_OK, "create clock probe");
        Completion completion;
        auto callbacks = mc_output_callbacks MC_OUTPUT_CALLBACKS_DEFAULT;
        callbacks.context = &completion;
        callbacks.completed = Completed;
        callbacks.ready = Ready;
        const auto frames = options.format.sample_rate / 10;
        options.queue_capacity_frames = frames * 2;
        std::vector<float> samples(frames * options.format.channels, 0.0f);
        auto block = mc_output_buffer MC_OUTPUT_BUFFER_DEFAULT;
        block.token = 1;
        block.pcm.data = samples.data();
        block.pcm.frame_count = frames;
        block.pcm.format = options.format;
        Require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "prepare clock probe");
        Require(api.pause(instance) == MC_OK, "pause clock probe");
        Require(api.queue_audio_buffer(instance, &block) == MC_OK, "queue while clock paused");
        auto timing = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
        Require(api.get_timing(instance, &timing) == MC_OK && (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED) &&
                (timing.valid_flags & MC_OUTPUT_TIMING_QUEUED) && (timing.valid_flags & MC_OUTPUT_TIMING_ESTIMATED),
            "timing includes estimated audible cursor and queue");
        Require(timing.frames_played_by_driver == 0 && timing.queued_seconds >= 0.099 &&
                std::isfinite(timing.queued_seconds),
            "paused queued audio remains pending");
        const auto generation = timing.generation;
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
        Require(api.get_timing(instance, &timing) == MC_OK && timing.frames_played_by_driver == 0,
            "paused clock does not consume queued audio");
        Require(api.resume(instance) == MC_OK, "resume clock probe");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            const auto result = api.get_timing(instance, &timing);
            Require(std::chrono::steady_clock::now() < deadline, "native clock remains readable");
            if (result == MC_ERROR_BUSY) {
                continue;
            }
            Require(result == MC_OK, "sample native clock");
            Require(timing.frames_played_by_driver <= frames, "clock excludes underrun silence");
            Require(std::chrono::steady_clock::now() < deadline, "audible tail reaches deadline");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while (timing.frames_played_by_driver < frames);
        Require(api.drain(instance, nullptr) == MC_OK, "drain clock probe");
        Require(completion.count.load() == 1, "clock does not change token ownership");
        Require(api.stop(instance) == MC_OK, "stop clock probe");
        Require(api.get_timing(instance, &timing) == MC_OK && timing.valid_flags == 0, "stopped clock is unknown");
        options.queue_capacity_frames = frames * 4;
        Require(api.prepare(instance, &options, &callbacks, nullptr) == MC_OK, "reprepare clock probe");
        Require(api.get_timing(instance, &timing) == MC_OK && timing.generation != generation &&
                timing.frames_played_by_driver == 0,
            "reprepare resets cursor and changes generation");
        /* queue normally, without the pre-pause used above. this exercises
        the running-device timestamp path rather than paused queue estimates. */
        /* supply enough initial buffers for native startup priming. */
        for (unsigned i = 0; i < 3; ++i) {
            ++block.token;
            Require(api.queue_audio_buffer(instance, &block) == MC_OK, "queue on running clock");
        }
        bool partialProgress = false;
        bool queuedRunningTail = false;
        uint64_t totalFrames = frames * 3;
        uint64_t previous = 0;
        const auto runningDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            const auto result = api.get_timing(instance, &timing);
            Require(std::chrono::steady_clock::now() < runningDeadline, "running clock reaches audible tail");
            if (result == MC_ERROR_BUSY) {
                continue;
            }
            Require(result == MC_OK && (timing.valid_flags & MC_OUTPUT_TIMING_PLAYED),
                "read running clock");
            Require(timing.frames_played_by_driver >= previous && timing.frames_played_by_driver <= totalFrames,
                "running clock advances monotonically through accepted PCM");
            previous = timing.frames_played_by_driver;
            partialProgress |= previous > 0 && previous < frames;
            if (previous >= frames / 2 && !queuedRunningTail) {
                /* the first enqueue can precede a valid native queue timeline.
                add another block after playback advances to sample it running. */
                ++block.token;
                Require(api.queue_audio_buffer(instance, &block) == MC_OK, "queue with a running native timeline");
                totalFrames += frames;
                queuedRunningTail = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while (timing.frames_played_by_driver < totalFrames);
        Require(partialProgress && queuedRunningTail, "running clock reports progress before the whole buffer completes");
        Require(api.drain(instance, nullptr) == MC_OK && completion.count.load() == 5,
            "running timing preserves complete native playback");
        Require(api.stop(instance) == MC_OK, "stop fresh clock generation");
        api.destroy(instance);
    }
}
