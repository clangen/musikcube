#include "Pcm.h"
#include "PcmPacket.h"
#include "SettingsPacket.h"
#include "Renderer.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/socket.h>

namespace {
    /* receives audio without blocking the renderer or the playback process. */
    class HelperInput {
    public:
        explicit HelperInput(int socket) : socket(socket) {}

        ProjectMSettings LoadSettings() {
            ProjectMSettings settings;
            size_t received = 0;
            for (;;) {
                ProjectMSettingsChunk chunk {};
                const auto count = recv(socket, &chunk, sizeof(chunk), 0);
                if (count < 0 && errno == EINTR) {
                    continue;
                }
                if (count <= 0) {
                    stopping = true;
                    return settings;
                }
                if (ReceiveProjectMSettings(chunk, static_cast<size_t>(count), settings, received)) {
                    return settings;
                }
            }
        }

        void SaveSettings(const ProjectMSettings& settings) noexcept {
            latestSettings = settings;
            settingsChanged = true;
        }

        void FlushSettings() noexcept {
            if (!sendingSettings && settingsChanged) {
                outgoingSettings = latestSettings;
                sentSettings = 0;
                sendingSettings = true;
                settingsChanged = false;
            }
            if (sendingSettings && SendProjectMSettings(socket, outgoingSettings, sentSettings, MSG_DONTWAIT)) {
                sendingSettings = false;
            }
        }

        void Started(bool success) noexcept {
            if (!success) {
                const char response = 0;
                (void) send(socket, &response, 1, 0);
            }
        }

        void Presented() noexcept {
            if (!ready) {
                /* the parent reports success only after a frame is presented. */
                const char response = 1;
                (void) send(socket, &response, 1, 0);
                ready = true;
            }
            FlushSettings();
        }

        bool Stopping() const noexcept { return stopping; }
        void Stop() noexcept { stopping = true; }
        void Finished() noexcept {
            FlushSettings();
            const char response = 0;
            (void) send(socket, &response, 1, MSG_DONTWAIT);
        }

        void ReadAudio(projectm_handle visualizer, float gain) noexcept {
            for (unsigned packets = 0; packets < 128; ++packets) {
                ProjectMPcmPacket packet {};
                const auto received = recv(socket, &packet, sizeof(packet), MSG_DONTWAIT);
                if (received < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                        stopping = true;
                    }
                    break;
                }
                if (received == 0) {
                    stopping = true;
                    break;
                }
                const auto header = offsetof(ProjectMPcmPacket, samples);
                if (static_cast<size_t>(received) < header || packet.frames > ProjectMPcmPacket::MaxFrames ||
                    static_cast<size_t>(received) != header + packet.frames * 2 * sizeof(float))
                {
                    continue;
                }
                ScaleVisualizerPcm(packet.samples, packet.frames * 2, gain);
                projectm_pcm_add_float(visualizer, packet.samples, packet.frames, PROJECTM_STEREO);
            }
        }

    private:
        ProjectMSettings latestSettings;
        ProjectMSettings outgoingSettings;
        size_t sentSettings { 0 };
        bool settingsChanged { false };
        bool sendingSettings { false };
        int socket;
        bool ready { false };
        bool stopping { false };
    };

    void HelperLocation() {}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    const int socket = std::atoi(argv[1]);
    HelperInput input(socket);
    RunProjectMRenderer(input, reinterpret_cast<void*>(&HelperLocation));
    return 0;
}
