#include <musikcore/plugin/playback-remote.hpp>
#include <musikcore/plugin/library.h>

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

namespace {
    /* record lifecycle calls where the test can read them after plugin unload. */
    void record(const char* text) noexcept {
        if (const char* path = std::getenv("MUSIKCORE_TEST_REMOTE_LOG")) {
            if (auto* file = std::fopen(path, "a")) {
#ifdef MUSIKCORE_TEST_REMOTE_PEER
                std::fputs("peer-", file);
#endif
                std::fputs(text, file);
                std::fclose(file);
            }
        }
    }

    /* a small remote that exercises the public C++ plugin helper. */
    class TestRemote {
    public:
        explicit TestRemote(const mc_plugin_services& services)
        : host(*services.playback_remote), library(*services.library) {
            record("create\n");
            /* constructors cannot enqueue work before the host starts the remote. */
            this->preStartRejected = host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.5, Completed, this) ==
                MC_ERROR_CANCELLED;
        }
        ~TestRemote() { record("destroy\n"); }

        /* report volume changes so the test can verify real event delivery. */
        void Changed(const mc_plugin_playback_event& event) noexcept {
            this->CheckOwner();
            if (event.event == MC_PLAYBACK_CHANGED_VOLUME) {
                record("volume\n");
            }
        }
        void QueueChanged() noexcept { this->CheckOwner(); record("queue\n"); }

        /* record the order in which the engine owns this remote. */
        mc_result Start() {
            this->CheckOwner();
            record("start\n");
            if (!this->preStartRejected) {
                return MC_ERROR_PLUGIN;
            }
            if (std::getenv("MUSIKCORE_TEST_REMOTE_FAIL")) {
                return MC_ERROR_IO;
            }
            /* the owner can provide the initial state before any change event. */
            if (host.read_state(host.context, InitialState, this) != MC_OK) {
                return MC_ERROR_PLUGIN;
            }
            if (std::getenv("MUSIKCORE_TEST_REMOTE_ACCESS")) {
                /* queue access from a worker; the owner supplies a separate handle. */
                mc_result result = MC_ERROR_INTERNAL;
                std::thread worker([&] {
                    result = host.with_playback_async(host.context, PlaybackAccess, this);
                });
                worker.join();
                return result;
            }
            if (std::getenv("MUSIKCORE_TEST_REMOTE_LIBRARY")) {
                if (this->library.list_category_fields(nullptr, LibraryFields, this) != MC_ERROR_INVALID_ARGUMENT ||
                    this->library.list_category_fields(this->library.context, nullptr, this) != MC_ERROR_INVALID_ARGUMENT ||
                    this->library.delete_playlist(this->library.context, 0) != MC_ERROR_INVALID_ARGUMENT)
                {
                    return MC_ERROR_PLUGIN;
                }
                /* library work runs on a network worker and returns its fields inline. */
                mc_result result = MC_ERROR_INTERNAL;
                std::thread reader([&] {
                    result = this->library.list_category_fields(this->library.context, LibraryFields, this);
                    if (result == MC_OK) {
                        result = this->library.delete_playlist(this->library.context, 999999);
                    }
                });
                reader.join();
                return result;
            }
#if !defined(_WIN32) && defined(MUSIKCORE_TEST_REMOTE_PEER)
            if (const char* readFd = std::getenv("MUSIKCORE_TEST_REMOTE_STARTFAIL_READ_FD")) {
                /* failed startup leaves a worker waiting for the first remote's completion. */
                const int fd = std::atoi(readFd);
                this->worker = std::thread([this, fd] {
                    {
                        std::lock_guard lock(this->mutex);
                        this->submitted = true;
                        this->changed.notify_all();
                    }
                    pollfd gate { fd, POLLIN, 0 };
                    char byte;
                    record(poll(&gate, 1, 5000) == 1 && read(fd, &byte, 1) == 1 ? "released\n" : "timeout\n");
                });
                std::unique_lock lock(this->mutex);
                if (!this->changed.wait_for(lock, std::chrono::seconds(5), [&] { return this->submitted; })) {
                    return MC_ERROR_INTERNAL;
                }
                return MC_ERROR_IO;
            }
#endif
#if !defined(_WIN32) && !defined(MUSIKCORE_TEST_REMOTE_PEER)
            if (const char* readFd = std::getenv("MUSIKCORE_TEST_REMOTE_READ_FD")) {
                /* this worker exits only after the other remote receives stop. */
                const int fd = std::atoi(readFd);
                this->worker = std::thread([this, fd] {
                    {
                        std::lock_guard lock(this->mutex);
                        this->submitted = true;
                        this->changed.notify_all();
                    }
                    pollfd gate { fd, POLLIN, 0 };
                    char byte;
                    record(poll(&gate, 1, 5000) == 1 && read(fd, &byte, 1) == 1 ? "released\n" : "timeout\n");
                });
                std::unique_lock lock(this->mutex);
                return this->changed.wait_for(lock, std::chrono::seconds(5), [&] { return this->submitted; })
                    ? MC_OK : MC_ERROR_INTERNAL;
            }
#endif
            /* invalid volume is rejected before the command reaches playback. */
            if (host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 2.0, Completed, this) !=
                MC_ERROR_INVALID_ARGUMENT)
            {
                return MC_ERROR_PLUGIN;
            }
            if (std::getenv("MUSIKCORE_TEST_REMOTE_RACE")) {
                /* keep submitting until stop closes admission, with a bounded queue. */
                this->raceMode = true;
                this->worker = std::thread([this] {
                    while (!this->stopping.load()) {
                        const auto status = host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.42, Completed, this);
                        if (status == MC_OK) {
                            std::lock_guard lock(this->mutex);
                            ++this->accepted;
                            this->changed.notify_all();
                        }
                        else if (status == MC_ERROR_QUEUE_FULL) {
                            std::unique_lock lock(this->mutex);
                            const auto before = this->finished.load();
                            this->changed.wait_for(lock, std::chrono::seconds(5), [&] {
                                return this->stopping.load() || this->finished.load() != before;
                            });
                        }
                        else if (status == MC_ERROR_CANCELLED) {
                            break;
                        }
                        else {
                            record("submit-error\n");
                            break;
                        }
                    }
                });
                std::unique_lock lock(this->mutex);
                return this->changed.wait_for(lock, std::chrono::seconds(5), [&] { return this->accepted.load() > 0; })
                    ? MC_OK : MC_ERROR_INTERNAL;
            }
            if (std::getenv("MUSIKCORE_TEST_REMOTE_FLOOD")) {
                /* the host must bound work submitted before the owner dispatches. */
                for (unsigned i = 0; i < MC_PLAYBACK_REMOTE_MAX_PENDING_COMMANDS; ++i) {
                    if (host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.42, Completed, this) != MC_OK) {
                        return MC_ERROR_PLUGIN;
                    }
                }
                const auto result = host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.42, Completed, this);
                record(result == MC_ERROR_QUEUE_FULL ? "full\n" : "error\n");
                return result == MC_ERROR_QUEUE_FULL ? MC_OK : MC_ERROR_PLUGIN;
            }
            if (std::getenv("MUSIKCORE_TEST_REMOTE_HOLD")) {
                /* the worker cannot exit until shutdown settles its command. */
                this->worker = std::thread([this] {
                    const auto status = host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.42, Completed, this);
                    std::unique_lock lock(this->mutex);
                    this->submitted = true;
                    this->submission = status;
                    this->changed.notify_all();
                    if (status == MC_OK && !this->changed.wait_for(lock, std::chrono::seconds(5), [&] {
                        return this->completed;
                    })) {
                        record("timeout\n");
                    }
                });
                std::unique_lock lock(this->mutex);
                if (!this->changed.wait_for(lock, std::chrono::seconds(5), [&] { return this->submitted; })) {
                    return MC_ERROR_INTERNAL;
                }
                return this->submission == MC_OK && std::getenv("MUSIKCORE_TEST_REMOTE_FAIL_AFTER_SUBMIT")
                    ? MC_ERROR_IO : this->submission;
            }
            /* submit from a worker so the host must marshal the command to playback's owner. */
            mc_result result = MC_ERROR_INTERNAL;
            std::thread worker([&] {
                if (host.read_state(host.context, InitialState, this) != MC_ERROR_WRONG_THREAD) {
                    return;
                }
                result = host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.42, Completed, this);
            });
            worker.join();
            if (result == MC_OK && std::getenv("MUSIKCORE_TEST_REMOTE_DISCONNECT")) {
                /* simulate losing the client after the command was accepted. */
                record("disconnected\n");
            }
            return result;
        }
        void RequestStop() noexcept {
            this->CheckOwner();
            record("stop\n");
            {
                std::lock_guard lock(this->mutex);
                this->stopping.store(true);
                this->changed.notify_all();
            }
#if !defined(_WIN32) && defined(MUSIKCORE_TEST_REMOTE_PEER)
            if (const char* writeFd = std::getenv("MUSIKCORE_TEST_REMOTE_WRITE_FD")) {
                const char byte = 1;
                if (write(std::atoi(writeFd), &byte, 1) != 1) {
                    record("gate-error\n");
                }
            }
            if (const char* writeFd = std::getenv("MUSIKCORE_TEST_REMOTE_STARTFAIL_WAKE_FD")) {
                const char byte = 1;
                if (write(std::atoi(writeFd), &byte, 1) != 1) {
                    record("gate-error\n");
                }
            }
#endif
            /* once stopping begins, a late worker cannot publish another callback. */
            if (host.control_async(host.context, MC_PLAYBACK_SET_VOLUME, 0.5, Completed, this) != MC_ERROR_CANCELLED) {
                record("late-command-accepted\n");
            }
        }
        mc_result Join() {
            this->CheckOwner();
            if (this->worker.joinable()) {
                this->worker.join();
            }
            if (this->raceMode) {
                if (this->accepted.load() != this->finished.load()) {
                    record("unsettled\n");
                    return MC_ERROR_PLUGIN;
                }
                record("settled\n");
            }
            record("join\n");
            return MC_OK;
        }

    private:
        /* handles are transferred on success and must be destroyed by the plugin. */
        static void MC_CALL PlaybackAccess(void* data, mc_result result, mc_playback* playback) {
            auto& self = *static_cast<TestRemote*>(data);
            self.CheckOwner();
            if (result == MC_ERROR_CANCELLED && !playback) {
                record("access-cancelled\n");
                return;
            }
            mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
            const auto status = playback ? mc_playback_get_snapshot(playback, &snapshot) : MC_ERROR_INTERNAL;
            record(result == MC_OK && status == MC_OK ? "access-ok\n" : "access-error\n");
            mc_playback_destroy(&playback);
        }

        /* the callback receives views that are valid for this call only. */
        static void MC_CALL LibraryFields(mc_result result, const mc_string_view* fields,
            size_t count, const mc_error*, void*)
        {
            bool artist = false;
            for (size_t i = 0; i < count; ++i) {
                if (std::string_view(fields[i].data, fields[i].length) == "artist") {
                    artist = true;
                }
            }
            record(result == MC_OK && artist ? "library-fields\n" : "library-error\n");
        }

        /* the read is inline and provides a valid playback snapshot. */
        static void MC_CALL InitialState(void*, const mc_playback_snapshot* snapshot, const mc_track_ref*) {
            record(snapshot && snapshot->struct_size == sizeof(*snapshot) ? "initial\n" : "invalid-state\n");
        }

        /* the host settles an accepted command before destroying this instance. */
        static void MC_CALL Completed(void* data, mc_result result) {
            auto& self = *static_cast<TestRemote*>(data);
            self.CheckOwner();
            if (self.raceMode) {
                ++self.finished;
                if (result == MC_OK && !self.runningReported.exchange(true)) {
                    record("running\n");
                }
            }
            else {
                record(result == MC_OK ? "command\n" : result == MC_ERROR_CANCELLED ? "cancelled\n" : "error\n");
            }
            std::lock_guard lock(self.mutex);
            self.completed = true;
            self.changed.notify_all();
        }

        /* a callback on a worker would race teardown and break the SDK contract. */
        void CheckOwner() const noexcept {
            if (std::this_thread::get_id() != this->owner) {
                record("wrong-thread\n");
            }
        }

        const mc_playback_remote_host_api& host;
        const mc_library_host_api& library;
        const std::thread::id owner { std::this_thread::get_id() };
        std::thread worker;
        std::mutex mutex;
        std::condition_variable changed;
        mc_result submission { MC_ERROR_INTERNAL };
        bool submitted { false };
        bool completed { false };
        bool preStartRejected { false };
        bool raceMode { false };
        std::atomic<bool> stopping { false };
        std::atomic<unsigned> accepted { 0 };
        std::atomic<unsigned> finished { 0 };
        std::atomic<bool> runningReported { false };

    };

    mc_result MC_CALL initialize(const mc_host_api* host, void**, mc_error**) {
        if (std::getenv("MUSIKCORE_TEST_REMOTE_THROW") || std::getenv("MUSIKCORE_TEST_REMOTE_THROW_START")) {
            /* a broken plugin publishes an uncertain instance and throws across C. */
            static const mc_playback_remote_vtable methods {
                sizeof(mc_playback_remote_vtable),
                [](void*) { std::fputs("unsafe-destroy\n", stderr); },
                [](void*, const mc_plugin_playback_event*) {},
                [](void*) {},
                [](void*, mc_error**) -> mc_result { throw std::runtime_error("foreign start threw"); },
                [](void*) { std::fputs("unsafe-stop\n", stderr); },
                [](void*, mc_error**) -> mc_result { return MC_OK; },
            };
            static const mc_playback_remote_factory factory {
                sizeof(mc_playback_remote_factory), nullptr, &methods,
                [](void*, const mc_plugin_services*, void** out, mc_error**) -> mc_result {
                    *out = reinterpret_cast<void*>(1);
                    if (std::getenv("MUSIKCORE_TEST_REMOTE_THROW")) {
                        throw std::runtime_error("foreign factory threw");
                    }
                    return MC_OK;
                },
            };
            return musik::core::plugin::RegisterPlaybackRemote(*host, factory);
        }
        return musik::core::plugin::RegisterPlaybackRemote<TestRemote>(*host);
    }

    void MC_CALL shutdown(void*) { record("unload\n"); }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
#ifdef MUSIKCORE_TEST_REMOTE_PEER
    plugin->id = { "org.musikcube.test.playback-remote-peer", sizeof("org.musikcube.test.playback-remote-peer") - 1 };
#else
    plugin->id = { "org.musikcube.test.playback-remote", sizeof("org.musikcube.test.playback-remote") - 1 };
#endif
    plugin->name = { "Playback remote fixture", sizeof("Playback remote fixture") - 1 };
    plugin->version = { "1", 1 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
