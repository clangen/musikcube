#include <musikcore/plugin.h>
#include <musikcore/plugin/output.hpp>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>

namespace {
    using Clock = std::chrono::steady_clock;
    constexpr char PluginId[] = "0d45a986-24f1-4253-9fc2-b432353a1eea";
    constexpr char DeviceId[] = "null";
    constexpr size_t DefaultCapacity = 4096;

    bool knownDevice(mc_string_view device) {
        return !device.length ||
            (device.data && device.length == sizeof(DeviceId) - 1 &&
                std::memcmp(device.data, DeviceId, sizeof(DeviceId) - 1) == 0);
    }

    /* discard PCM at its playback rate on the producer's thread. the only
    shared work is an interruptible wait: Stop wakes it and waits for completion
    callbacks. ordinary calls are serialized; Stop may overlap a write or drain.
    instances cannot be copied or moved and own no threads or audio storage. */
    class NullOutput {
    public:
        NullOutput() = default;
        NullOutput(const NullOutput&) = delete;
        NullOutput(NullOutput&&) = delete;

        ~NullOutput() = default;

        NullOutput& operator=(const NullOutput&) = delete;
        NullOutput& operator=(NullOutput&&) = delete;

        mc_result Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
            if (!knownDevice(options.device_id)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            this->format = options.format;
            this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
            this->volume = options.initial_volume;
            this->callbacks = callbacks;
            this->paused = false;
            this->stopped = false;
            return MC_OK;
        }

        mc_result QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
            std::unique_lock<std::mutex> lock(this->mutex);
            if (this->stopped) {
                return MC_ERROR_CANCELLED;
            }
            const auto& pcm = buffer.pcm;
            if (pcm.format.sample_rate != this->format.sample_rate || pcm.format.channels != this->format.channels ||
                pcm.format.channel_layout != this->format.channel_layout)
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            if (pcm.frame_count > this->capacity) {
                return MC_ERROR_BUFFER_TOO_SMALL;
            }
            if (this->paused) {
                return MC_ERROR_QUEUE_FULL;
            }
            if (this->active) {
                return MC_ERROR_IN_FLIGHT;
            }

            /* validate the deadline before accepting ownership. no buffer,
            token, or clock state needs to survive this producer call. */
            const auto now = Clock::now();
            const auto ticks = std::ceil(static_cast<long double>(pcm.frame_count) * Clock::period::den /
                this->format.sample_rate / Clock::period::num);
            if (ticks > (Clock::time_point::max() - now).count()) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            this->active = true;
            try {
                this->wake.wait_until(lock, now + Clock::duration(static_cast<Clock::rep>(ticks)), [&] {
                    return this->stopped;
                });
            }
            catch (...) {
                this->active = false;
                this->wake.notify_all();
                return MC_ERROR_INTERNAL;
            }
            const auto outcome = this->stopped ? MC_ERROR_CANCELLED : MC_OK;
            lock.unlock();
            this->Complete(buffer.token, outcome);
            lock.lock();
            this->active = false;
            this->wake.notify_all();
            /* acceptance succeeded even when Stop cancelled the paced wait.
            the completion outcome tells the host what happened to the token. */
            return MC_OK;
        }

        mc_result Pause() {
            if (this->stopped) {
                return MC_ERROR_CANCELLED;
            }
            this->paused = true;
            return MC_OK;
        }

        mc_result Resume() noexcept {
            if (this->stopped) {
                return MC_ERROR_CANCELLED;
            }
            const bool wasPaused = this->paused;
            this->paused = false;
            if (wasPaused && this->callbacks.ready) {
                this->callbacks.ready(this->callbacks.context);
            }
            return MC_OK;
        }

        mc_result Stop() noexcept {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->stopped = true;
            this->wake.notify_all();
            this->wake.wait(lock, [&] {
                return !this->active;
            });
            return MC_OK;
        }

        mc_result Drain() {
            std::lock_guard<std::mutex> lock(this->mutex);
            /* serialized writes finish before Drain; there is no device queue. */
            return this->stopped ? MC_ERROR_CANCELLED : MC_OK;
        }

        mc_result SetVolume(double value) {
            this->volume = value;
            return MC_OK;
        }

        double GetVolume() const { return this->volume; }

        static mc_result VisitDevices(mc_output_device_fn read, void* user) {
            const mc_output_device device { { DeviceId, sizeof(DeviceId) - 1 }, { "Null Output", 11 } };
            read(user, &device);
            return MC_OK;
        }

        static mc_result ReadDefaultDevice(mc_output_device_fn read, void* user) { return VisitDevices(read, user); }

        mc_result GetDefaultFormat(mc_string_view device, mc_audio_format& format) {
            if (!knownDevice(device)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            format = { sizeof(format), 48000, 2, 0 };
            return MC_OK;
        }

    private:
        /* host callbacks must not throw or reenter stop/destroy. failed ownership
        return cannot be treated as a recoverable operation error. */
        void Complete(uint64_t token, mc_result outcome) noexcept {
            this->callbacks.completed(this->callbacks.context, token, outcome);
        }

        double volume = 1;
        std::mutex mutex;
        std::condition_variable wake;
        mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
        size_t capacity = DefaultCapacity;
        bool stopped = true;
        bool paused = false;
        bool active = false;
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error** error) {
        if (error) {
            *error = nullptr;
        }
        *state = nullptr;
        return musik::core::plugin::RegisterOutput<NullOutput>(*host);
    }

    void MC_CALL shutdown(void*) {
        /* instances have already been destroyed before registry shutdown. */
    }
}

/* one immutable factory can serve independent engine sessions. entry and
initialize start no workers; buffer pacing runs on the calling producer thread. */
extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { PluginId, sizeof(PluginId) - 1 };
    plugin->name = { "Null Output", 11 };
    plugin->version = { "1", 1 };
    plugin->author = { "musikcube", 9 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
