#include "ProjectMBackend.h"
#include "Pcm.h"
#include "PcmQueue.h"
#include "Renderer.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
    void PluginLocation() {}
}

/* Linux and Windows own the renderer on its own worker thread. */
class ProjectMBackend::Impl {
public:
    ~Impl() noexcept { Hide(); }

    mc_result Show() {
        std::lock_guard lock(control);
        if (visible.load()) {
            return MC_OK;
        }
        if (worker.joinable()) {
            worker.join();
        }
        stopping.store(false);
        {
            std::lock_guard startupLock(startup);
            ready = false;
            started = false;
        }
        worker = std::thread([this] {
            RunProjectMRenderer(*this, reinterpret_cast<void*>(&PluginLocation));
        });
        std::unique_lock startupLock(startup);
        changed.wait(startupLock, [this] { return ready; });
        return started ? MC_OK : MC_ERROR_IO;
    }

    mc_result Hide() noexcept {
        std::lock_guard lock(control);
        stopping.store(true);
        if (worker.joinable()) {
            worker.join();
        }
        return MC_OK;
    }

    mc_result IsVisible(bool& value) const {
        value = visible.load();
        return MC_OK;
    }

    mc_result WritePcm(const mc_pcm_view& pcm) noexcept {
        if (visible.load(std::memory_order_relaxed)) {
            queue.Write(pcm);
        }
        return MC_OK;
    }

    void Started(bool success) {
        {
            std::lock_guard lock(startup);
            visible.store(success, std::memory_order_release);
            started = success;
            ready = true;
        }
        changed.notify_all();
    }

    void Presented() noexcept {}
    bool Stopping() const noexcept { return stopping.load(std::memory_order_relaxed); }
    void Stop() noexcept { stopping.store(true); }
    void Finished() noexcept { visible.store(false, std::memory_order_release); }

    void ReadAudio(projectm_handle visualizer, float gain) noexcept {
        queue.Drain([&](float* samples, size_t frames) {
            ScaleVisualizerPcm(samples, frames * 2, gain);
            projectm_pcm_add_float(visualizer, samples, static_cast<unsigned>(frames), PROJECTM_STEREO);
        });
    }

private:
    std::mutex control;
    std::mutex startup;
    std::condition_variable changed;
    std::thread worker;
    bool ready = false;
    bool started = false;
    std::atomic<bool> stopping { false };
    std::atomic<bool> visible { false };
    ProjectMPcmQueue queue;
};

ProjectMBackend::ProjectMBackend(const mc_plugin_services&) : impl(std::make_unique<Impl>()) {}
ProjectMBackend::~ProjectMBackend() noexcept = default;
mc_result ProjectMBackend::Show() { return impl->Show(); }
mc_result ProjectMBackend::Hide() noexcept { return impl->Hide(); }
mc_result ProjectMBackend::IsVisible(bool& visible) const { return impl->IsVisible(visible); }
mc_result ProjectMBackend::WritePcm(const mc_pcm_view& pcm) noexcept { return impl->WritePcm(pcm); }
