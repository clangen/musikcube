#include <musikcore/plugin/output.hpp>
#include <musikcore/plugin/services.h>
#include <mutex>
#include <condition_variable>
#include <array>
#include <string>
#include <string_view>
#include <cstdlib>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    bool environmentFlag(const char* name) {
#ifdef _WIN32
        return GetEnvironmentVariableA(name, nullptr, 0) != 0;
#else
        return std::getenv(name) != nullptr;
#endif
    }
    /* this fixture is loaded by one host per process. log calls reveal whether
    the real engine boundary forwards invalid work or inserts cleanup in order. */
    const mc_host_api* host = nullptr;

    void record(const char* message, size_t size) {
        host->log(host->host_context, MC_LOG_INFO, { "output-test", 11 }, { message, size });
    }

    /* the test can unplug the pinned USB device between selection and Prepare. */
    bool usbConnected() {
        return !environmentFlag("MUSIKCORE_TEST_OUTPUT_USB_UNPLUGGED");
    }

    class RecordingOutput {
    public:
        explicit RecordingOutput(const mc_plugin_services* services) : services(services) { record("create", 6); }
        ~RecordingOutput() { record("destroy", 7); }

        mc_result Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
            record("prepare", 7);
            std::lock_guard lock(this->mutex);
            this->callbacks = callbacks;
            this->mode.assign(options.device_id.data ? options.device_id.data : "", options.device_id.length);
            this->stopped = false;
            if (this->mode == "usb" && !usbConnected()) {
                return MC_ERROR_NOT_FOUND;
            }
            return this->mode == "fail" ? MC_ERROR_IO : MC_OK;
        }

        mc_result QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
            record("queue", 5);
            std::unique_lock lock(this->mutex);
            if (this->stopped) {
                return MC_ERROR_CANCELLED;
            }
            /* a device error can return the second block while the first
            still belongs to the driver until recovery stops it. */
            if (this->mode == "fail-second" && this->pendingCount) {
                lock.unlock();
                this->callbacks.completed(this->callbacks.context, buffer.token, MC_ERROR_IO);
                return MC_OK;
            }
            if (this->mode == "hold" || this->mode == "fail-second") {
                this->pending[this->pendingCount++] = buffer.token;
                return MC_OK;
            }
            if (this->mode == "ready") {
                if (this->capacityPending) {
                    return MC_ERROR_QUEUE_FULL;
                }
                if (!this->capacityReturned) {
                    this->capacityPending = true;
                    this->completionWorker = std::thread([this, token = buffer.token] {
                        /* storage can return before the driver frees space.
                        the host logger holds this worker at that handoff. */
                        this->callbacks.completed(this->callbacks.context, token, MC_OK);
                        record("capacity-pending", 16);
                        {
                            std::lock_guard lock(this->mutex);
                            this->capacityPending = false;
                            this->capacityReturned = true;
                        }
                        if (!environmentFlag("MUSIKCORE_TEST_OMIT_OUTPUT_READY")) {
                            this->callbacks.ready(this->callbacks.context);
                        }
                        record("capacity-ready", 14);
                    });
                    return MC_OK;
                }
            }
            if (this->mode == "block") {
                this->active = true;
                record("blocked", 7);
                this->changed.wait(lock, [&] {
                    return this->stopped;
                });
                lock.unlock();
                this->callbacks.completed(this->callbacks.context, buffer.token, MC_ERROR_CANCELLED);
                lock.lock();
                this->active = false;
                this->changed.notify_all();
                return MC_OK;
            }
            lock.unlock();
            if (this->mode == "stale" && this->previousToken) {
                this->callbacks.completed(this->callbacks.context, this->previousToken, MC_OK);
            }
            this->previousToken = buffer.token;
            this->callbacks.completed(this->callbacks.context, buffer.token, MC_OK);
            if (this->mode == "duplicate") {
                this->callbacks.completed(this->callbacks.context, buffer.token, MC_OK);
            }
            return MC_OK;
        }

        mc_result Stop() noexcept {
            record("stop", 4);
            if (environmentFlag("MUSIKCORE_TEST_FAIL_OUTPUT_STOP")) {
                return MC_ERROR_IO;
            }
            std::unique_lock lock(this->mutex);
            this->stopped = true;
            this->changed.notify_all();
            this->changed.wait(lock, [&] {
                return !this->active;
            });
            const auto pending = this->pending;
            const auto count = std::exchange(this->pendingCount, 0);
            lock.unlock();
            if (this->completionWorker.joinable()) {
                this->completionWorker.join();
            }
            for (size_t i = 0; i < count; ++i) {
                this->callbacks.completed(this->callbacks.context, pending[i], MC_ERROR_CANCELLED);
            }
            return MC_OK;
        }
        mc_result Pause() {
            record("pause", 5);
            return MC_OK;
        }
        mc_result Resume() {
            record("resume", 6);
            return MC_OK;
        }
        mc_result SetVolume(double) {
            record("volume", 6);
            return MC_OK;
        }
        double GetVolume() const { return 1.0; }
        mc_result Flush() { return MC_ERROR_UNSUPPORTED; }
        mc_result Drain() {
            record("drain", 5);
            return MC_OK;
        }

        static mc_result VisitDevices(mc_output_device_fn read, void* data) {
            ReadDefaultDevice(read, data);
            /* these fixture devices exercise specific driver behaviors. */
            for (const std::string_view id : { "fail", "fail-second", "hold", "ready", "block", "stale", "duplicate" }) {
                const mc_string_view text { id.data(), id.size() };
                const mc_output_device device { text, text };
                read(data, &device);
            }
            if (usbConnected()) {
                const mc_output_device usb { { "usb", 3 }, { "USB", 3 } };
                read(data, &usb);
            }
            return MC_OK;
        }
        static mc_result ReadDefaultDevice(mc_output_device_fn read, void* data) {
            const mc_output_device device { { "builtin", 7 }, { "Speakers", 8 } };
            read(data, &device);
            return MC_OK;
        }
        mc_result GetDefaultFormat(mc_string_view id, mc_audio_format& format) {
            if (environmentFlag("MUSIKCORE_TEST_OUTPUT_PREFERENCES")) {
                if (!services || !services->preferences) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                bool enabled = false;
                const auto* prefs = services->preferences;
                const auto result = prefs->read_boolean(prefs->context, { "fixture", 7 }, false, &enabled);
                if (result != MC_OK || !enabled) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                format = { sizeof(format), 44100, 2, 0 };
                return MC_OK;
            }
            const bool usb = id.length && std::string_view(id.data, id.length) == "usb";
            if (usb && !usbConnected()) {
                return MC_ERROR_NOT_FOUND;
            }
            format = { sizeof(format), usb ? 44100u : 48000u, 2, 0 };
            return MC_OK;
        }

    private:
        const mc_plugin_services* services;
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
        std::mutex mutex;
        std::condition_variable changed;
        std::thread completionWorker;
        std::array<uint64_t, 64> pending {};
        size_t pendingCount = 0;
        uint64_t previousToken = 0;
        std::string mode;
        bool stopped = true;
        bool active = false;
        bool capacityPending = false;
        bool capacityReturned = false;
    };

    mc_output_factory recordedFactory = MC_OUTPUT_FACTORY_DEFAULT;
    decltype(mc_output_factory::create) originalCreate = nullptr;

    /* emulate the C contract that permits a Stop-safe partial object on
    failed creation; the ordinary C++ helper always cleans constructor failure. */
    mc_result MC_CALL create(void* context, const mc_plugin_services* services, void** instance, mc_error** error) {
        if (environmentFlag("MUSIKCORE_TEST_OUTPUT_PREFERENCES")) {
            if (!services || !services->preferences || !services->preferences->read_boolean) {
                return MC_ERROR_PLUGIN;
            }
            bool value = false;
            const mc_string_view key { "fixture", 7 };
            if (services->preferences->read_boolean(services->preferences->context, key, false, &value) != MC_OK ||
                !value)
            {
                return MC_ERROR_PLUGIN;
            }
        }
        const auto result = originalCreate(context, services, instance, error);
        return result == MC_OK && environmentFlag("MUSIKCORE_TEST_PARTIAL_OUTPUT_CREATE") ? MC_ERROR_IO : result;
    }

    /* preserve the immutable helper table while overriding only this fixture's
    creation outcome. production host validation still sees the full C table. */
    mc_result MC_CALL registerOutput(void*, const mc_plugin_capability* capability) {
        recordedFactory = *static_cast<const mc_output_factory*>(capability->api);
        originalCreate = recordedFactory.create;
        recordedFactory.create = create;
        auto record = *capability;
        record.api = &recordedFactory;
        return host->register_plugin_capability(host->host_context, &record);
    }

    mc_result MC_CALL initialize(const mc_host_api* api, void** state, mc_error**) {
        host = api;
        *state = nullptr;
        auto fixtureHost = *api;
        fixtureHost.register_plugin_capability = registerOutput;
        return musik::core::plugin::RegisterOutput<RecordingOutput>(fixtureHost);
    }
    void MC_CALL shutdown(void*) {
        host = nullptr;
    }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error) {
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "output.test", 11 };
    plugin->name = { "Recording Output", 16 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
