#include "Pcm.h"
#include "PcmPacket.h"
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
        }

        bool Stopping() const noexcept { return stopping; }
        void Stop() noexcept { stopping = true; }
        void Finished() noexcept {}

        void ReadAudio(projectm_handle visualizer, float gain, float& peak) noexcept {
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
                peak = std::max(peak, ScaleVisualizerPcm(packet.samples, packet.frames * 2, gain));
                projectm_pcm_add_float(visualizer, packet.samples, packet.frames, PROJECTM_STEREO);
            }
        }

    private:
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
    fcntl(socket, F_SETFL, fcntl(socket, F_GETFL) | O_NONBLOCK);
    HelperInput input(socket);
    RunProjectMRenderer(input, reinterpret_cast<void*>(&HelperLocation));
    return 0;
}
