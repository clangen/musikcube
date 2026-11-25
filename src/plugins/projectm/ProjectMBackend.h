#pragma once

#include <musikcore/plugin/visualizer.h>
#include <memory>

/* provides a window and audio input using the current platform's lifecycle. */
class ProjectMBackend {
public:
    static constexpr mc_visualizer_inputs Inputs = MC_VISUALIZER_INPUT_PCM;
    static const char* Name() { return "projectM"; }

    explicit ProjectMBackend(const mc_plugin_services&);
    ~ProjectMBackend() noexcept;

    ProjectMBackend(const ProjectMBackend&) = delete;
    ProjectMBackend& operator=(const ProjectMBackend&) = delete;

    mc_result Show();
    mc_result Hide() noexcept;
    mc_result IsVisible(bool&) const;
    mc_result WritePcm(const mc_pcm_view&) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
