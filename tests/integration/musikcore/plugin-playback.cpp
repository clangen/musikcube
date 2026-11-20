#include "CoreRuntime.h"

#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/Outputs.h>
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
        const auto available = outputs::GetAllOutputs();
        if (available.size() != 1 || available.front()->Name() != nullOutput) {
            std::fputs("Expected only the headless output plugin\n", stderr);
            return false;
        }

        outputs::SelectOutput(available.front());
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
            outputs::SelectedOutput()->Name() != nullOutput)
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
