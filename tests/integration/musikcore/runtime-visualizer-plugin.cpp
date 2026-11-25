/* Headless C visualizer used to verify discovery and session teardown. */
#include <musikcore/plugin.h>
#include <musikcore/plugin/visualizer.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
    class Visualizer {
    public:
        static constexpr mc_visualizer_inputs Inputs = MC_VISUALIZER_INPUT_PCM | MC_VISUALIZER_INPUT_SPECTRUM;
        static const char* Name() { return "Headless runtime visualizer"; }

        explicit Visualizer(const mc_plugin_services& services)
        : marker(std::filesystem::u8path(std::string(services.data_directory.data, services.data_directory.length)) /
            "visualizer-lifecycle.txt") {}
        ~Visualizer() noexcept { Record('R'); }

        mc_result Show() { visible.store(true); return MC_OK; }
        mc_result Hide() { visible.store(false); Record('H'); return MC_OK; }
        mc_result IsVisible(bool& value) { value = visible.load(); return MC_OK; }
        mc_result WritePcm(const mc_pcm_view& pcm) {
            if (pcm.frame_count != 1 || pcm.format.channels != 2) return MC_ERROR_INVALID_ARGUMENT;
            Record('P');
            return MC_OK;
        }
        mc_result WriteSpectrum(const float*, size_t count, uint32_t rate) {
            if (count != 2 || rate != 44100) return MC_ERROR_INVALID_ARGUMENT;
            Record('S');
            return MC_OK;
        }

    private:
        void Record(char event) noexcept {
            try { std::ofstream out(marker, std::ios::app); out << event; } catch (...) {}
        }
        std::filesystem::path marker;
        std::atomic<bool> visible { false };
    };

    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterVisualizer<Visualizer>(*host);
    }
    void MC_CALL Shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* api, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, api, error);
    if (result != MC_OK) return result;
    *api = MC_PLUGIN_API_DEFAULT;
    api->id = { "runtime-visualizer", 18 };
    api->name = { "RuntimeVisualizer", 17 };
    api->version = { "1", 1 };
    api->author = { "musikcube tests", 15 };
    api->initialize = Initialize;
    api->shutdown = Shutdown;
    return MC_OK;
}

/* A dual-entry module must be discovered only through its C capability.
The old loader must not initialize its legacy visualizer entry point. */
extern "C" MC_PLUGIN_API void* GetPlugin() { std::abort(); }
extern "C" MC_PLUGIN_API void* GetPcmVisualizer() { std::abort(); }
