#include "../CopiedOutput.h"
#include <musikcore/plugin/output.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <string_view>
#include <vector>

namespace {
    constexpr uint32_t Rate = CopiedOutputSampleRate;
    constexpr uint32_t Channels = 2;
    std::mutex mutex;
    std::condition_variable changed;
    mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    std::vector<float> samples;
    uint64_t accepted = 0;
    uint64_t played = 0;
    uint64_t returned = 0;
    std::atomic<size_t> queued { 0 };
    std::atomic_bool knownTiming { true };
    std::atomic_bool callbackCompletion { false };
    std::atomic<uint64_t> playedSnapshot { 0 };
    struct Pending {
        uint64_t token;
        uint64_t endFrame;
    };
    std::vector<Pending> pending;
    constexpr uint64_t DeviceLeadFrames = 2048;
    bool full = false;
    bool draining = false;
    bool stopped = true;

    class Output {
    public:
        mc_result Prepare(const mc_output_options& options, const mc_output_callbacks& supplied) {
            if (options.format.sample_rate != Rate || options.format.channels != Channels) {
                return MC_ERROR_UNSUPPORTED;
            }
            std::lock_guard lock(mutex);
            samples.resize(options.queue_capacity_frames * Channels);
            callbacks = supplied;
            knownTiming = std::string_view(options.device_id.data, options.device_id.length) != "unknown";
            callbackCompletion = std::string_view(options.device_id.data, options.device_id.length) == "callback";
            playedSnapshot = 0;
            pending.clear();
            pending.reserve(64);
            accepted = played = returned = 0;
            queued = 0;
            full = draining = stopped = false;
            return MC_OK;
        }
        mc_result QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
            {
                std::lock_guard lock(mutex);
                const auto capacity = samples.size() / Channels;
                if (stopped) {
                    return MC_ERROR_CANCELLED;
                }
                if (buffer.pcm.frame_count > capacity) {
                    return MC_ERROR_BUFFER_TOO_SMALL;
                }
                if (buffer.pcm.frame_count > capacity - queued.load()) {
                    full = true;
                    changed.notify_all();
                    return MC_ERROR_QUEUE_FULL;
                }
                /* reuse of the host's buffer cannot change this device copy. */
                for (size_t i = 0; i < buffer.pcm.frame_count * Channels; ++i) {
                    samples[(accepted * Channels + i) % samples.size()] = buffer.pcm.data[i];
                }
                accepted += buffer.pcm.frame_count;
                queued = accepted - played;
                if (callbackCompletion && accepted > played + DeviceLeadFrames) {
                    pending.push_back({ .token = buffer.token, .endFrame = accepted });
                    return MC_OK;
                }
                ++returned;
            }
            callbacks.completed(callbacks.context, buffer.token, MC_OK);
            return MC_OK;
        }
        mc_result Stop() noexcept {
            std::unique_lock lock(mutex);
            stopped = true;
            queued = 0;
            auto cancelled = std::move(pending);
            changed.notify_all();
            lock.unlock();
            for (const auto& block : cancelled) {
                callbacks.completed(callbacks.context, block.token, MC_ERROR_CANCELLED);
            }
            return MC_OK;
        }
        mc_result Drain() {
            std::unique_lock lock(mutex);
            draining = true;
            changed.notify_all();
            changed.wait(lock, [] {
                return stopped || !queued.load();
            });
            return stopped ? MC_ERROR_CANCELLED : MC_OK;
        }
        mc_result GetTiming(mc_output_timing& timing) const noexcept {
            timing = MC_OUTPUT_TIMING_DEFAULT;
            timing.queued_seconds = std::numeric_limits<double>::quiet_NaN();
            if (knownTiming.load()) {
                timing.valid_flags = MC_OUTPUT_TIMING_QUEUED;
                timing.queued_seconds = static_cast<double>(queued.load()) / Rate;
                if (callbackCompletion) {
                    timing.valid_flags |= MC_OUTPUT_TIMING_PLAYED;
                    timing.generation = 1;
                    timing.frames_played_by_driver = playedSnapshot.load();
                    timing.frames_played_timestamp_ns = 1;
                }
            }
            return MC_OK;
        }
        mc_result Pause() { return MC_OK; }
        mc_result Resume() { return MC_OK; }
        mc_result SetVolume(double value) {
            this->volume = value;
            return MC_OK;
        }
        double GetVolume() const { return this->volume; }
        mc_result GetDefaultFormat(mc_string_view, mc_audio_format& format) {
            format = {
                .struct_size = sizeof(format),
                .sample_rate = Rate,
                .channels = Channels,
                .channel_layout = 0,
            };
            return MC_OK;
        }
        mc_result Flush() { return MC_ERROR_UNSUPPORTED; }
        static mc_result VisitDevices(mc_output_device_fn read, void* data) {
            for (const std::string_view id : { "known", "unknown", "callback" }) {
                const mc_output_device device {
                    .id = { id.data(), id.size() },
                    .name = { id.data(), id.size() },
                };
                read(data, &device);
            }
            return MC_OK;
        }
        static mc_result ReadDefaultDevice(mc_output_device_fn, void*) { return MC_ERROR_NOT_FOUND; }

    private:
        double volume = 1;
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterOutput<Output>(*host);
    }
    void MC_CALL shutdown(void*) {
    }
}

extern "C" MC_PLUGIN_API CopiedOutputSnapshot ReadCopiedOutputState() {
    std::lock_guard lock(mutex);
    return {
        .acceptedFrames = accepted,
        .playedFrames = played,
        .returnedBuffers = returned,
        .queuedFrames = queued.load(),
        .draining = draining,
    };
}

extern "C" MC_PLUGIN_API bool WaitCopiedOutputBlocked(uint64_t previousAccepted, unsigned milliseconds) {
    std::unique_lock lock(mutex);
    return changed.wait_for(lock, std::chrono::milliseconds(milliseconds), [&] {
        return ((full || draining) && accepted > previousAccepted) ||
            (callbackCompletion && accepted == CopiedOutputTrackFrames);
    });
}

extern "C" MC_PLUGIN_API void AdvanceCopiedOutputDevice(size_t frames) {
    std::vector<uint64_t> completed;
    {
        std::lock_guard lock(mutex);
        const auto count = std::min(frames, queued.load());
        /* the generated WAV contains a constant half-scale signal. checking
        the stored copy catches accidental retention of recycled decoder data. */
        for (size_t i = 0; i < count * Channels; ++i) {
            if (samples[(played * Channels + i) % samples.size()] != 0.5f) {
                std::abort();
            }
        }
        played += count;
        playedSnapshot = played;
        queued = accepted - played;
        while (!pending.empty() && pending.front().endFrame <= played + DeviceLeadFrames) {
            completed.push_back(pending.front().token);
            pending.erase(pending.begin());
            ++returned;
        }
        full = false;
        changed.notify_all();
    }
    for (const auto token : completed) {
        callbacks.completed(callbacks.context, token, MC_OK);
    }
    callbacks.ready(callbacks.context);
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error) {
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "test.copied.output", 18 };
    plugin->name = { "Copied output", 13 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
