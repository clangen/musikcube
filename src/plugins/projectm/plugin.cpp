#include "Pcm.h"
#include "PresetDirectory.h"
#include <musikcore/plugin/visualizer.hpp>
#include <projectM-4/projectM.h>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <thread>

namespace {
    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error** error);

    constexpr size_t BufferFrames = 2048;
    constexpr size_t BufferCount = 4;
    constexpr char Preset[] = R"([preset00]
fRating=3.0
fDecay=0.97
nWaveMode=7
bAdditiveWaves=1
wave_x=0.5
wave_y=0.5
per_frame_1=wave_r=0.5+0.5*sin(time*0.7);wave_g=0.5+0.5*sin(time*0.9);wave_b=0.5+0.5*sin(time*1.1);
)";

    class ProjectMVisualizer {
    public:
        static constexpr mc_visualizer_inputs Inputs = MC_VISUALIZER_INPUT_PCM;
        static const char* Name() { return "projectM"; }
        explicit ProjectMVisualizer(const mc_plugin_services&) {}
        ~ProjectMVisualizer() noexcept { Hide(); }

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
                Run();
            });
            std::unique_lock startupLock(startup);
            changed.wait(startupLock, [this] {
                return ready;
            });
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

        /* PCM callbacks only claim preallocated slots; a busy renderer drops data. */
        mc_result WritePcm(const mc_pcm_view& pcm) noexcept {
            if (!visible.load(std::memory_order_relaxed) || !pcm.data || !pcm.format.channels) {
                return MC_OK;
            }
            for (auto& block : buffers) {
                unsigned expected = 0;
                if (!block.state.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
                    continue;
                }
                block.frames = std::min(pcm.frame_count, BufferFrames);
                for (size_t i = 0; i < block.frames; ++i) {
                    block.samples[i * 2] = pcm.data[i * pcm.format.channels];
                    block.samples[i * 2 + 1] = pcm.data[i * pcm.format.channels + (pcm.format.channels > 1 ? 1 : 0)];
                }
                block.state.store(2, std::memory_order_release);
                break;
            }
            return MC_OK;
        }

    private:
        struct Block {
            std::atomic<unsigned> state { 0 };
            std::array<float, BufferFrames * 2> samples {};
            size_t frames = 0;
        };

        void SignalStarted(bool success) {
            {
                std::lock_guard lock(startup);
                started = success;
                ready = true;
            }
            changed.notify_all();
        }

        void Run() noexcept {
            SDL_Window* window = nullptr;
            SDL_GLContext context = nullptr;
            projectm_handle visualizer = nullptr;
            bool initialized = false;
            bool imguiContext = false;
            bool imguiPlatform = false;
            bool imguiRenderer = false;

            if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
                initialized = true;
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                window = SDL_CreateWindow("musikcube - projectM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960,
                    540, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
                if (window) {
                    context = SDL_GL_CreateContext(window);
                }
                if (context) {
                    visualizer = projectm_create();
                }
            }

            if (visualizer) {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                imguiContext = true;
                ImGui::GetIO().IniFilename = nullptr;
                ImGui::StyleColorsDark();
                imguiPlatform = ImGui_ImplSDL2_InitForOpenGL(window, context);
                if (imguiPlatform) {
                    imguiRenderer = ImGui_ImplOpenGL3_Init("#version 330");
                }
            }

            const bool success = visualizer && imguiRenderer;
            std::vector<std::string> presets;
            size_t current = 0;
            bool advanceRequested = false;
            bool controlsVisible = true;
            bool automatic = true;
            bool fullscreen = false;
            float duration = 30.0f;
            float inputGain = 4.0f;
            float inputPeak = 0.0f;
            std::array<char, 4096> presetDirectory {};
            if (const char* path = std::getenv("MUSIKCUBE_PROJECTM_PRESETS")) {
                std::strncpy(presetDirectory.data(), path, presetDirectory.size() - 1);
            }
            else {
                const auto bundled = ProjectMPresetDirectory(reinterpret_cast<void*>(&Initialize)).string();
                std::strncpy(presetDirectory.data(), bundled.c_str(), presetDirectory.size() - 1);
            }

            auto loadPresets = [&] {
                presets.clear();
                std::error_code error;
                std::filesystem::recursive_directory_iterator entry(
                    presetDirectory.data(), std::filesystem::directory_options::skip_permission_denied, error);
                const std::filesystem::recursive_directory_iterator end;
                while (!error && entry != end) {
                    if (entry->is_regular_file(error) && entry->path().extension() == ".milk") {
                        presets.push_back(entry->path().string());
                    }
                    entry.increment(error);
                }
                std::sort(presets.begin(), presets.end());
                current = 0;
                if (!presets.empty()) {
                    projectm_load_preset_file(visualizer, presets[current].c_str(), false);
                }
            };
            auto switchPreset = [&](int direction) {
                if (!presets.empty()) {
                    current = direction > 0 ? (current + 1) % presets.size()
                                            : (current + presets.size() - 1) % presets.size();
                    projectm_load_preset_file(visualizer, presets[current].c_str(), true);
                }
            };

            if (success) {
                projectm_load_preset_data(visualizer, Preset, false);
                projectm_set_preset_duration(visualizer, duration);
                projectm_set_preset_switch_requested_event_callback(
                    visualizer,
                    [](bool, void* user) {
                        *static_cast<bool*>(user) = true;
                    },
                    &advanceRequested);
                loadPresets();
                SDL_GL_SetSwapInterval(1);
                visible.store(true, std::memory_order_release);
            }
            SignalStarted(success);
            while (success && !stopping.load(std::memory_order_relaxed)) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    ImGui_ImplSDL2_ProcessEvent(&event);
                    if (event.type == SDL_QUIT ||
                        (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
                    {
                        stopping.store(true);
                    }
                    if (event.type == SDL_KEYDOWN && !event.key.repeat && !ImGui::GetIO().WantCaptureKeyboard) {
                        switch (event.key.keysym.sym) {
                            case SDLK_ESCAPE:
                                stopping.store(true);
                                break;
                            case SDLK_TAB:
                                controlsVisible = !controlsVisible;
                                break;
                            case SDLK_LEFT:
                                switchPreset(-1);
                                break;
                            case SDLK_RIGHT:
                                switchPreset(1);
                                break;
                            case SDLK_SPACE:
                                automatic = !automatic;
                                projectm_set_preset_locked(visualizer, !automatic);
                                break;
                            case SDLK_f:
                                fullscreen = !fullscreen;
                                SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                                break;
                        }
                    }
                }
                if (advanceRequested) {
                    advanceRequested = false;
                    switchPreset(1);
                }
                inputPeak *= 0.95f;
                for (auto& block : buffers) {
                    unsigned expected = 2;
                    if (block.state.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
                        inputPeak =
                            std::max(inputPeak, ScaleVisualizerPcm(block.samples.data(), block.frames * 2, inputGain));
                        projectm_pcm_add_float(
                            visualizer, block.samples.data(), static_cast<unsigned>(block.frames), PROJECTM_STEREO);
                        block.state.store(0, std::memory_order_release);
                    }
                }
                int width = 0;
                int height = 0;
                SDL_GL_GetDrawableSize(window, &width, &height);
                if (width > 0 && height > 0) {
                    projectm_set_window_size(visualizer, static_cast<size_t>(width), static_cast<size_t>(height));
                    projectm_opengl_render_frame(visualizer);
                    ImGui_ImplOpenGL3_NewFrame();
                    ImGui_ImplSDL2_NewFrame();
                    ImGui::NewFrame();
                    if (controlsVisible) {
                        ImGui::SetNextWindowBgAlpha(0.8f);
                        ImGui::Begin("projectM controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                        ImGui::InputText("Preset directory", presetDirectory.data(), presetDirectory.size());
                        if (ImGui::Button("Load presets")) {
                            loadPresets();
                        }
                        ImGui::SameLine();
                        ImGui::Text("%zu presets", presets.size());
                        if (!presets.empty()) {
                            ImGui::TextUnformatted(std::filesystem::path(presets[current]).filename().string().c_str());
                        }
                        if (ImGui::Button("Previous")) {
                            switchPreset(-1);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Next")) {
                            switchPreset(1);
                        }
                        if (ImGui::Checkbox("Automatic advance", &automatic)) {
                            projectm_set_preset_locked(visualizer, !automatic);
                        }
                        if (ImGui::SliderFloat("Duration (seconds)", &duration, 5.0f, 300.0f, "%.0f")) {
                            projectm_set_preset_duration(visualizer, duration);
                        }
                        ImGui::SliderFloat("Audio gain", &inputGain, 1.0f, 8.0f, "%.2fx");
                        ImGui::ProgressBar(std::min(inputPeak, 1.0f), ImVec2(180, 0), "Input level");
                        ImGui::SameLine();
                        ImGui::Text("%.2f", inputPeak);
                        ImGui::TextUnformatted("Left/Right: preset  Space: auto  F: fullscreen");
                        ImGui::TextUnformatted("Tab: controls  Escape: close");
                        ImGui::End();
                    }
                    ImGui::Render();
                    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                    SDL_GL_SwapWindow(window);
                }
                SDL_Delay(8);
            }
            visible.store(false, std::memory_order_release);
            if (imguiRenderer) {
                ImGui_ImplOpenGL3_Shutdown();
            }
            if (imguiPlatform) {
                ImGui_ImplSDL2_Shutdown();
            }
            if (imguiContext) {
                ImGui::DestroyContext();
            }
            if (visualizer) {
                projectm_destroy(visualizer);
            }
            if (context) {
                SDL_GL_DeleteContext(context);
            }
            if (window) {
                SDL_DestroyWindow(window);
            }
            if (initialized) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            }
        }

        std::mutex control;
        std::mutex startup;
        std::condition_variable changed;
        std::thread worker;
        bool ready = false;
        bool started = false;
        std::atomic<bool> stopping { false };
        std::atomic<bool> visible { false };
        std::array<Block, BufferCount> buffers;
    };

    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterVisualizer<ProjectMVisualizer>(*host);
    }

    void MC_CALL Shutdown(void*) {
    }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error) {
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "org.musikcube.visualizer.projectm", sizeof("org.musikcube.visualizer.projectm") - 1 };
    plugin->name = { "projectM", 8 };
    plugin->version = { "4.1.8", 5 };
    plugin->initialize = Initialize;
    plugin->shutdown = Shutdown;
    return MC_OK;
}
