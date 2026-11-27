#include "ProjectMBackend.h"
#include "PcmPacket.h"
#include "PcmQueue.h"
#include "SettingsPacket.h"
#include <musikcore/plugin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace {
    void PluginLocation() {}
}

class ProjectMBackend::Impl {
public:
    static constexpr mc_visualizer_inputs Inputs = MC_VISUALIZER_INPUT_PCM;
    static const char* Name() { return "projectM"; }
    explicit Impl(const mc_plugin_services& services)
    : settingsService(services.settings)
    , host(services.host) {}
    ~Impl() noexcept { Hide(); }

    mc_result Show() {
        std::lock_guard lock(this->control);
        if (this->visible.load()) {
            return MC_OK;
        }
        this->Stop();
        Dl_info info {};
        if (!dladdr(reinterpret_cast<void*>(&PluginLocation), &info) || !info.dli_fname) {
            return MC_ERROR_IO;
        }
        const auto helper = std::filesystem::absolute(info.dli_fname).parent_path() / "musikcube-projectm";
        const auto path = helper.string();
        int sockets[2];
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) != 0) {
            return MC_ERROR_IO;
        }
        int enabled = 1;
        int bufferSize = 65536;
        if (setsockopt(sockets[0], SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0 ||
            fcntl(sockets[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(sockets[1], F_SETFD, FD_CLOEXEC) != 0)
        {
            close(sockets[0]);
            close(sockets[1]);
            return MC_ERROR_IO;
        }
        setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
        setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));

        posix_spawn_file_actions_t actions;
        if (posix_spawn_file_actions_init(&actions) != 0) {
            close(sockets[0]);
            close(sockets[1]);
            return MC_ERROR_IO;
        }
        constexpr int childSocket = 3;
        const auto closeParent = posix_spawn_file_actions_addclose(&actions, sockets[0]);
        const auto duplicate = posix_spawn_file_actions_adddup2(&actions, sockets[1], childSocket);
        const auto closeChild = sockets[1] != childSocket
            ? posix_spawn_file_actions_addclose(&actions, sockets[1]) : 0;
        /* projectM and the graphics driver can write diagnostics while the
        terminal UI is drawing; keep those out of the terminal. */
        const auto quietStdout = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        const auto quietStderr = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        if (closeParent || duplicate || closeChild || quietStdout || quietStderr) {
            posix_spawn_file_actions_destroy(&actions);
            close(sockets[0]);
            close(sockets[1]);
            return MC_ERROR_IO;
        }
        char socketArgument[] = "3";
        char* arguments[] = { const_cast<char*>(path.c_str()), socketArgument, nullptr };
        pid_t child = -1;
        const auto status = posix_spawn(&child, path.c_str(), &actions, nullptr, arguments, environ);
        posix_spawn_file_actions_destroy(&actions);
        close(sockets[1]);
        if (status != 0) {
            close(sockets[0]);
            return MC_ERROR_IO;
        }
        this->socket = sockets[0];
        this->child = child;
        auto settings = LoadProjectMSettings(this->settingsService);
        size_t sent = 0;
        if (!SendProjectMSettings(this->socket, settings, sent, 0)) {
            this->Stop();
            return MC_ERROR_IO;
        }
        pollfd descriptor { .fd = this->socket, .events = POLLIN, .revents = 0 };
        char response = 0;
        if (poll(&descriptor, 1, 10000) <= 0 || recv(this->socket, &response, 1, 0) != 1 || response != 1) {
            this->Stop();
            return MC_ERROR_IO;
        }
        this->visible.store(true, std::memory_order_release);
        try {
            this->sender = std::thread([this] { this->SendAudio(); });
        }
        catch (...) {
            this->Stop();
            throw;
        }
        return MC_OK;
    }

    mc_result Hide() noexcept {
        std::lock_guard lock(this->control);
        this->Stop();
        return MC_OK;
    }

    mc_result IsVisible(bool& value) const {
        value = this->visible.load(std::memory_order_acquire);
        return MC_OK;
    }

    /* a busy visualizer drops frames rather than delaying audio playback. */
    mc_result WritePcm(const mc_pcm_view& pcm) noexcept {
        if (this->visible.load(std::memory_order_relaxed)) {
            this->queue.Write(pcm);
        }
        return MC_OK;
    }

private:
    bool ReadSettings() noexcept {
        ProjectMSettingsChunk chunk {};
        ssize_t count;
        while ((count = recv(this->socket, &chunk, sizeof(chunk), MSG_DONTWAIT)) > 0) {
            if (count == 1) {
                return false;
            }
            if (ReceiveProjectMSettings(
                    chunk, static_cast<size_t>(count), this->incomingSettings, this->receivedSettings))
            {
                if (SaveProjectMSettings(this->settingsService, this->incomingSettings) != MC_OK && this->host &&
                    this->host->log)
                {
                    const char message[] = "could not save visualizer settings";
                    this->host->log(
                        this->host->host_context, MC_LOG_WARNING, { "projectM", 8 }, { message, sizeof(message) - 1 });
                }
            }
        }
        return count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
    }

    void SendAudio() noexcept {
        while (this->visible.load(std::memory_order_acquire)) {
            /* poll only while the window is open, keeping socket operations
            off the playback callback and detecting a closed helper. */
            pollfd descriptor { .fd = this->socket, .events = POLLIN, .revents = 0 };
            const auto status = poll(&descriptor, 1, 8);
            if (status < 0 && errno != EINTR) {
                break;
            }
            if (status > 0 && (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))) {
                break;
            }
            if (status > 0 && (descriptor.revents & POLLIN)) {
                if (!this->ReadSettings()) {
                    break;
                }
            }
            this->queue.Drain([&](const float* input, size_t frames) {
                for (size_t offset = 0; offset < frames; offset += ProjectMPcmPacket::MaxFrames) {
                    ProjectMPcmPacket packet {};
                    packet.frames = static_cast<uint32_t>(std::min(ProjectMPcmPacket::MaxFrames, frames - offset));
                    const auto samples = packet.frames * 2;
                    std::memcpy(packet.samples, input + offset * 2, samples * sizeof(float));
                    const auto bytes = offsetof(ProjectMPcmPacket, samples) + samples * sizeof(float);
                    if (send(this->socket, &packet, bytes, MSG_DONTWAIT) < 0 &&
                        errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR && errno != ENOBUFS)
                    {
                        this->visible.store(false, std::memory_order_release);
                        break;
                    }
                }
            });
        }
        this->visible.store(false, std::memory_order_release);
    }

    void Stop() noexcept {
        this->visible.store(false, std::memory_order_release);
        if (this->sender.joinable()) {
            this->sender.join();
        }
        if (this->socket >= 0) {
            this->ReadSettings();
            shutdown(this->socket, SHUT_RDWR);
            close(this->socket);
            this->socket = -1;
        }
        if (this->child > 0) {
            int status;
            pid_t result;
            do {
                result = waitpid(this->child, &status, WNOHANG);
            } while (result < 0 && errno == EINTR);
            if (result == 0) {
                kill(this->child, SIGKILL);
                while (waitpid(this->child, &status, 0) < 0 && errno == EINTR) {}
            }
            this->child = -1;
        }
    }

    const mc_plugin_settings_service* settingsService;
    const mc_host_api* host;
    ProjectMSettings incomingSettings;
    size_t receivedSettings { 0 };
    std::mutex control;
    std::thread sender;
    std::atomic<bool> visible { false };
    ProjectMPcmQueue queue;
    int socket { -1 };
    pid_t child { -1 };
};


ProjectMBackend::ProjectMBackend(const mc_plugin_services& services)
: impl(std::make_unique<Impl>(services)) {
}
ProjectMBackend::~ProjectMBackend() noexcept = default;
mc_result ProjectMBackend::Show() { return impl->Show(); }
mc_result ProjectMBackend::Hide() noexcept { return impl->Hide(); }
mc_result ProjectMBackend::IsVisible(bool& visible) const { return impl->IsVisible(visible); }
mc_result ProjectMBackend::WritePcm(const mc_pcm_view& pcm) noexcept { return impl->WritePcm(pcm); }
