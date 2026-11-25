#include <musikcore/musikcore.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 3) {
        return 1;
    }
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.plugin_directory = { argv[1], std::strlen(argv[1]) };
    options.services = MC_SERVICE_PLUGINS;
    mc_context* context = nullptr;
    mc_player* player = nullptr;
    if (mc_context_create(&options, &context, nullptr) != MC_OK ||
        mc_player_create(context, &player) != MC_OK ||
        mc_player_play_uri(player, { argv[2], std::strlen(argv[2]) }) != MC_OK)
    {
        std::fputs("installed decoder consumer could not start playback\n", stderr);
        return 1;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    mc_player_snapshot snapshot = MC_PLAYER_SNAPSHOT_DEFAULT;
    while (std::chrono::steady_clock::now() < deadline) {
        if (mc_context_dispatch(context) != MC_OK || mc_player_get_snapshot(player, &snapshot) != MC_OK ||
            snapshot.state == MC_PLAYER_FAILED)
        {
            break;
        }
        if (snapshot.state == MC_PLAYER_FINISHED) {
            mc_player_destroy(&player);
            return mc_context_destroy(&context) == MC_OK ? 0 : 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::fprintf(stderr, "installed decoder playback failed or timed out (state %u)\n", snapshot.state);
    mc_player_destroy(&player);
    mc_context_destroy(&context);
    return 1;
}
