#include <musikcore/musikcore.hpp>
#include "../../support/host-wakeup.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Linux output runtime: %s\n", message);
        std::abort();
    }
}

static void littleEndian(std::ostream& output, uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        output.put(static_cast<char>((value >> (i * 8)) & 255));
    }
}

int main(int argc, char** argv) {
    require(argc == 3, "expected driver and device");
    const auto directory = std::filesystem::absolute(argv[0]).parent_path();
    const auto filename = directory / "silence.wav";
    constexpr unsigned rate = 48000;
    const std::vector<float> samples(rate * 2 * 2, 0.0f);
    {
        std::ofstream file(filename, std::ios::binary);
        const auto bytes = static_cast<uint32_t>(samples.size() * sizeof(float));
        file.write("RIFF", 4);
        littleEndian(file, bytes + 36, 4);
        file.write("WAVEfmt ", 8);
        littleEndian(file, 16, 4);
        littleEndian(file, 3, 2);
        littleEndian(file, 2, 2);
        littleEndian(file, rate, 4);
        littleEndian(file, rate * 8, 4);
        littleEndian(file, 8, 2);
        littleEndian(file, 32, 2);
        file.write("data", 4);
        littleEndian(file, bytes, 4);
        file.write(reinterpret_cast<const char*>(samples.data()), bytes);
        require(bool(file), "write WAV fixture");
    }
    const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> wake(
        test_host_event_create(), test_host_event_destroy);
    require(bool(wake), "create owner wakeup");
    const auto plugins = (directory / "plugins").string();
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_PLAYBACK;
    options.plugin_directory = { plugins.data(), plugins.size() };
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = wake.get();
    musik::core::api::Context context(options);
    if (std::string_view(argv[1]) == "AlsaOut") {
        constexpr char pluginId[] = "668a75e6-1816-4c75-a361-a9d48906f23f";
        constexpr char key[] = "latency_ms";
        mc_preference_entry setting {};
        setting.key = { key, sizeof(key) - 1 };
        setting.value.type = MC_PREFERENCE_INTEGER;
        setting.value.data.integer = 5001;
        require(mc_plugin_write_setting({ pluginId, sizeof(pluginId) - 1 }, &setting) == MC_ERROR_INVALID_ARGUMENT,
            "reject latency above the declared range");
        setting.value.data.integer = 9;
        require(mc_plugin_write_setting({ pluginId, sizeof(pluginId) - 1 }, &setting) == MC_ERROR_INVALID_ARGUMENT,
            "reject latency below the declared range");
        setting.value.data.integer = 125;
        require(mc_plugin_write_setting({ pluginId, sizeof(pluginId) - 1 }, &setting) == MC_OK,
            "save valid ALSA latency");
    }
    require(musik::core::api::SelectOutputDevice(argv[1], argv[2]) == MC_OK, "select native output");
    require(musik::core::api::ReadOutputConfiguration().selectedDriver == argv[1], "selected output is active");
    musik::core::api::Player player(context);
    require(player.Play(filename.string()) == MC_OK, "start playback");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    for (;;) {
        const auto state = player.Snapshot().state;
        require(state != MC_PLAYER_FAILED, "playback pipeline failed");
        if (state == MC_PLAYER_FINISHED) {
            break;
        }
        require(std::chrono::steady_clock::now() < deadline, "playback did not finish");
        test_host_event_wait_for(wake.get(), 100);
        require(context.Dispatch() == MC_OK, "dispatch playback events");
    }
    require(context.Shutdown() == MC_OK, "shutdown output runtime");
}
