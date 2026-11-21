#include "CoreRuntime.h"

#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/output.hpp>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Preferences.h>

#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>

namespace {
    using namespace musik::core;
    using namespace musik::core::audio;
    using namespace std::chrono_literals;
    using State = sdk::PlaybackState;

    constexpr char unavailableOutput[] = "Unavailable test output";
    constexpr std::string_view nullOutput = "Null";

    bool startPlayback(GaplessTransport& transport, const char* media) {
        transport.Start(media, ITransport::Gain {}, ITransport::StartMode::Immediate);
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (transport.GetPlaybackState() == State::Playing && transport.Position() > 0.0) {
                return true;
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    bool reloadDuringPlayback(const char* media) {
        const auto output = api::ReadOutputConfiguration();
        if (output.drivers.size() != 1 || output.drivers.front() != nullOutput ||
            output.selectedDriver != nullOutput)
        {
            std::fputs("Expected only the headless output plugin\n", stderr);
            return false;
        }

        if (api::SelectOutputDriver(nullOutput) != MC_OK ||
            api::SelectOutputDriver(unavailableOutput) != MC_ERROR_NOT_FOUND ||
            api::SelectOutputDevice(unavailableOutput, {}) != MC_ERROR_NOT_FOUND)
        {
            std::fputs("Output selection accepted an unavailable driver\n", stderr);
            return false;
        }

        if (api::SetOutputTransport(MC_OUTPUT_CROSSFADE) != MC_OK ||
            api::ReadOutputConfiguration().transport != MC_OUTPUT_CROSSFADE ||
            api::SetOutputTransport(MC_OUTPUT_GAPLESS) != MC_OK ||
            api::ReadOutputConfiguration().transport != MC_OUTPUT_GAPLESS)
        {
            std::fputs("Output configuration did not preserve the selected transport\n", stderr);
            return false;
        }

        GaplessTransport transport;
        if (!startPlayback(transport, media)) {
            std::fputs("Playback did not start with the headless output\n", stderr);
            return false;
        }

        /* Reload the output while a decoder worker still owns buffers. */
        const auto prefs = Preferences::ForComponent(prefs::components::Playback);
        prefs->SetString(prefs::keys::OutputPlugin, unavailableOutput);
        transport.ReloadOutput();
        if (transport.GetPlaybackState() != State::Stopped ||
            api::ReadOutputConfiguration().selectedDriver != nullOutput)
        {
            std::fputs("Reload did not stop playback and select the fallback output\n", stderr);
            return false;
        }

        if (!startPlayback(transport, media)) {
            std::fputs("Playback did not restart on the fallback output\n", stderr);
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }

    CoreRuntime runtime;
    plugin::Start(&runtime.Queue(), nullptr, LibraryFactory::Instance().DefaultLocalLibrary());
    const bool passed = reloadDuringPlayback(argv[1]);
    Player::WaitForWorkers();
    return passed ? 0 : 1;
}
