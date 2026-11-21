/* header-only C++ URI playback, with no catalog or indexing dependency.
usage: core-player <URI> [plugin-directory]. */
#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: core-player <URI> [plugin-directory]\n";
        return 1;
    }
    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
        event(test_host_event_create(), test_host_event_destroy);
    if (!event) {
        return 1;
    }
    try {
        namespace api = musik::core::api;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        if (argc == 3) {
            const std::string_view plugins(argv[2]);
            options.plugin_directory = { plugins.data(), plugins.size() };
        }
        /* state outlives both the player and context, including exception cleanup. */
        struct PlaybackState {
                bool done = false;
                bool failed = false;
        } state;
        api::Context context(options);
        api::Player player(context);
        player.SetCallback([](void* data, const mc_player_snapshot* snapshot) {
            auto& state = *static_cast<PlaybackState*>(data);
            state.failed = snapshot->state == MC_PLAYER_FAILED;
            state.done = state.failed || snapshot->state == MC_PLAYER_FINISHED;
        },
            &state);
        if (player.Play(argv[1]) != MC_OK) {
            return 1;
        }
        /* remain asleep between state changes, including for long tracks. */
        while (!state.done && test_host_event_wait_for(event.get(), UINT32_MAX)) {
            if (context.Dispatch() != MC_OK) {
                break;
            }
        }
        /* Close stops this resource; it does not wait on other player instances. */
        const auto closed = player.Close();
        return state.done && !state.failed && closed == MC_OK ? 0 : 1;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
