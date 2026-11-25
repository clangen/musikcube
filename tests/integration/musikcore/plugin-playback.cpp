#include "CoreRuntime.h"

#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/output.hpp>
#include <musikcore/playback.hpp>
#include <musikcore/visualizer.hpp>
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
    using State = musik::core::audio::PlaybackState;

    constexpr char unavailableOutput[] = "Unavailable test output";
    constexpr std::string_view nullOutput = "Null Output";

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
        if (!api::ListVisualizers().empty() || api::ShowVisualizer(0) != MC_ERROR_NOT_FOUND ||
            api::HideVisualizer() != MC_ERROR_NOT_FOUND)
        {
            std::fputs("An empty visualizer registry did not return empty results\n", stderr);
            return false;
        }

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
        if (api::SetPreamp(3.5) != MC_OK || api::SetReplayGain(MC_REPLAY_GAIN_ALBUM) != MC_OK) {
            std::fputs("Could not configure playback gain\n", stderr);
            return false;
        }
        const auto gain = api::ReadPlaybackGainSettings();
        if (api::SetPreamp(21.0) != MC_ERROR_INVALID_ARGUMENT ||
            gain.preamp_db != 3.5 || gain.replay_gain != MC_REPLAY_GAIN_ALBUM ||
            api::ReadPlaybackGainSettings().preamp_db != gain.preamp_db)
        {
            std::fputs("Playback gain did not round trip or reject an invalid value\n", stderr);
            return false;
        }

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

    /* output discovery must also work while the plugin service is offline. */
    if (!musik::core::audio::outputs::GetAllOutputs().empty()) {
        std::fputs("Found output plugins before startup\n", stderr);
        return 1;
    }

    {
        CoreRuntime runtime;
        musik::core::plugin::BindServices(&runtime.Queue());
        if (!reloadDuringPlayback(argv[1])) {
            return 1;
        }
    }

    if (!musik::core::audio::outputs::GetAllOutputs().empty()) {
        std::fputs("Found output plugins after shutdown\n", stderr);
        return 1;
    }
    return 0;
}
