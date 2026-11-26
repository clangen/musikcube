#pragma once

#include "PresetDirectory.h"
#include "PresetSearch.h"
#include <projectM-4/projectM.h>
#ifdef _WIN32
#include <GL/glew.h>
#endif
#include <SDL2/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

/* renders a window on the calling thread; input supplies audio and lifecycle. */
template<typename Input> void RunProjectMRenderer(Input& input, const void* symbol) {
    constexpr char Preset[] = R"([preset00]
fRating=3.0
fDecay=0.97
nWaveMode=7
bAdditiveWaves=1
wave_x=0.5
wave_y=0.5
per_frame_1=wave_r=0.5+0.5*sin(time*0.7);wave_g=0.5+0.5*sin(time*0.9);wave_b=0.5+0.5*sin(time*1.1);
)";
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
        window = SDL_CreateWindow("musikcube - projectM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 540,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
        if (window) {
            context = SDL_GL_CreateContext(window);
        }
        if (context) {
#ifdef _WIN32
            /* projectM uses GLEW on Windows, which needs the current context. */
            glewExperimental = GL_TRUE;
            if (glewInit() == GLEW_OK) {
                visualizer = projectm_create();
            }
#else
            visualizer = projectm_create();
#endif
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
    bool confirmClose = false;
    bool automatic = true;
    enum class Panel { None, Settings, Find };
    Panel panel = Panel::None;
    float controlsAlpha = 1.0f;
    std::array<char, 256> search {};
    std::mt19937 random { std::random_device {}() };
#ifdef PROJECTM_HAS_TRANSITION_CONTROLS
    bool reduceTransitionWork = true;
#endif
    bool fullscreen = false;
    float duration = 30.0f;
    float inputGain = 1.0f;
    std::array<char, 4096> presetDirectory {};
    if (const char* path = std::getenv("MUSIKCUBE_PROJECTM_PRESETS")) {
        std::strncpy(presetDirectory.data(), path, presetDirectory.size() - 1);
    }
    else {
        const auto bundled = ProjectMPresetDirectory(symbol).string();
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
    auto selectPreset = [&](size_t index) {
        if (index < presets.size()) {
            current = index;
            projectm_load_preset_file(visualizer, presets[current].c_str(), true);
        }
    };
    auto switchPreset = [&](int direction) {
        if (!presets.empty()) {
            selectPreset(
                direction > 0 ? (current + 1) % presets.size() : (current + presets.size() - 1) % presets.size());
        }
    };
    auto randomPreset = [&] {
        if (presets.empty()) {
            return;
        }
        size_t index = 0;
        if (presets.size() > 1) {
            index = std::uniform_int_distribution<size_t>(0, presets.size() - 2)(random);
            if (index >= current) {
                ++index;
            }
        }
        selectPreset(index);
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
#ifdef PROJECTM_HAS_TRANSITION_CONTROLS
        /* two presets render during a fade. update the outgoing one at half rate
        to leave more time for the incoming preset and controls. */
        projectm_opengl_set_outgoing_preset_frame_divisor(visualizer, 2);
#endif
        SDL_GL_SetSwapInterval(1);
    }
    input.Started(success);
    while (success && !input.Stopping()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL ||
                event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT)
            {
                controlsAlpha = 1.0f;
            }
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
            {
                input.Stop();
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat && confirmClose) {
                switch (event.key.keysym.sym) {
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_y:
                        input.Stop();
                        confirmClose = false;
                        break;
                    case SDLK_ESCAPE:
                    case SDLK_n:
                        confirmClose = false;
                        break;
                }
            }
            else if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_ESCAPE) {
                if (controlsVisible && panel != Panel::None) {
                    panel = Panel::None;
                }
                else {
                    confirmClose = true;
                }
            }
            else if (event.type == SDL_KEYDOWN && !event.key.repeat && !confirmClose &&
                !ImGui::GetIO().WantCaptureKeyboard)
            {
                switch (event.key.keysym.sym) {
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
                        randomPreset();
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
            if (automatic) {
                switchPreset(1);
            }
        }
        input.ReadAudio(visualizer, inputGain);
        int width = 0;
        int height = 0;
        SDL_GL_GetDrawableSize(window, &width, &height);
        if (width > 0 && height > 0) {
            projectm_set_window_size(visualizer, static_cast<size_t>(width), static_cast<size_t>(height));
            projectm_opengl_render_frame(visualizer);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            bool mouseOverControls = false;
            if (controlsVisible && controlsAlpha > 0.0f) {
                constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
                const float right = ImGui::GetIO().DisplaySize.x - 10.0f;
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, controlsAlpha);
                ImGui::SetNextWindowPos(ImVec2(right, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
                ImGui::SetNextWindowBgAlpha(0.8f * controlsAlpha);
                ImGui::Begin("projectM toolbar", nullptr, flags);
                if (ImGui::Button(automatic ? "Lock preset" : "Unlock preset")) {
                    automatic = !automatic;
                    projectm_set_preset_locked(visualizer, !automatic);
                }
                ImGui::SameLine();
                if (ImGui::Button("Previous")) {
                    switchPreset(-1);
                }
                ImGui::SameLine();
                if (ImGui::Button("Next")) {
                    switchPreset(1);
                }
                ImGui::SameLine();
                if (ImGui::Button("Random")) {
                    randomPreset();
                }
                ImGui::SameLine();
                if (ImGui::Button("Settings")) {
                    panel = panel == Panel::Settings ? Panel::None : Panel::Settings;
                }
                ImGui::SameLine();
                if (ImGui::Button("Find")) {
                    panel = panel == Panel::Find ? Panel::None : Panel::Find;
                }
                const auto toolbarPos = ImGui::GetWindowPos();
                const auto toolbarSize = ImGui::GetWindowSize();
                mouseOverControls = ImGui::IsMouseHoveringRect(toolbarPos,
                    ImVec2(toolbarPos.x + toolbarSize.x, toolbarPos.y + toolbarSize.y));
                const float panelTop = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() + 6.0f;
                ImGui::End();

                ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetIO().DisplaySize.y - 10.0f), ImGuiCond_Always,
                    ImVec2(0.0f, 1.0f));
                ImGui::SetNextWindowBgAlpha(0.8f * controlsAlpha);
                ImGui::Begin("Current preset", nullptr, flags | ImGuiWindowFlags_NoInputs);
                ImGui::PushTextWrapPos(std::max(100.0f, ImGui::GetIO().DisplaySize.x - 40.0f));
                const auto presetName = presets.empty() ? std::string("Built-in preset")
                                                        : std::filesystem::path(presets[current]).stem().string();
                ImGui::TextUnformatted(presetName.c_str());
                ImGui::PopTextWrapPos();
                ImGui::End();

                if (panel != Panel::None) {
                    ImGui::SetNextWindowPos(ImVec2(right, panelTop), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
                    ImGui::SetNextWindowBgAlpha(0.8f * controlsAlpha);
                    ImGui::Begin(panel == Panel::Settings ? "projectM settings" : "Find preset", nullptr, flags);
                    if (panel == Panel::Settings) {
                        ImGui::SetNextItemWidth(360.0f);
                        ImGui::InputText("Preset directory", presetDirectory.data(), presetDirectory.size());
                        if (ImGui::Button("Load presets")) {
                            loadPresets();
                        }
                        ImGui::SameLine();
                        ImGui::Text("%zu presets", presets.size());
                        if (ImGui::SliderFloat("Duration (seconds)", &duration, 5.0f, 300.0f, "%.0f")) {
                            projectm_set_preset_duration(visualizer, duration);
                        }
#ifdef PROJECTM_HAS_TRANSITION_CONTROLS
                        if (ImGui::Checkbox("Reduce transition work", &reduceTransitionWork)) {
                            projectm_opengl_set_outgoing_preset_frame_divisor(visualizer, reduceTransitionWork ? 2 : 1);
                        }
#endif
                        ImGui::SliderFloat("Audio gain", &inputGain, 1.0f, 8.0f, "%.2fx");
                    }
                    else {
                        ImGui::SetNextItemWidth(360.0f);
                        ImGui::InputText("Search", search.data(), search.size());
                        ImGui::BeginChild("Preset results", ImVec2(450, 300), true);
                        for (size_t i = 0; i < presets.size(); ++i) {
                            const auto name = std::filesystem::path(presets[i]).filename().string();
                            if (ProjectMPresetMatches(name, search.data())) {
                                ImGui::PushID(static_cast<int>(i));
                                if (ImGui::Selectable(name.c_str(), i == current)) {
                                    selectPreset(i);
                                }
                                ImGui::PopID();
                            }
                        }
                        ImGui::EndChild();
                    }
                    const auto panelPos = ImGui::GetWindowPos();
                    const auto panelSize = ImGui::GetWindowSize();
                    mouseOverControls = mouseOverControls || ImGui::IsMouseHoveringRect(panelPos,
                        ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y));
                    ImGui::End();
                }
                ImGui::PopStyleVar();
            }
            /* ImGui retains mouse capture while a widget is active, even when
            the pointer leaves it during a drag. */
            if (panel != Panel::None && !mouseOverControls && !ImGui::GetIO().WantCaptureMouse) {
                for (int button = 0; button < 3; ++button) {
                    if (ImGui::IsMouseClicked(button)) {
                        panel = Panel::None;
                        break;
                    }
                }
            }
            constexpr const char* closeDialog = "Close projectM?";
            if (confirmClose && !ImGui::IsPopupOpen(closeDialog)) {
                ImGui::OpenPopup(closeDialog);
            }
            const auto displaySize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f),
                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal(closeDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (!confirmClose) {
                    ImGui::CloseCurrentPopup();
                }
                else {
                    ImGui::TextUnformatted("Are you sure you want to close?");
                    const auto& style = ImGui::GetStyle();
                    const float buttonsWidth = ImGui::CalcTextSize("Yes").x + ImGui::CalcTextSize("No").x +
                        style.FramePadding.x * 4.0f + style.ItemSpacing.x;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                        std::max(0.0f, ImGui::GetContentRegionAvail().x - buttonsWidth));
                    if (ImGui::Button("Yes")) {
                        input.Stop();
                        confirmClose = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("No")) {
                        confirmClose = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
            /* keep open panels visible even when the pointer and keyboard are idle. */
            if (panel != Panel::None || confirmClose) {
                controlsAlpha = 1.0f;
            }
            else if (!ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput) {
                controlsAlpha = std::max(0.0f, controlsAlpha - ImGui::GetIO().DeltaTime);
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);
            input.Presented();
        }
        SDL_Delay(8);
    }
    input.Finished();
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
