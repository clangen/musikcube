/* play one filesystem path, file:// URI, or provider URI without a catalog.
build against the installed C SDK and the small host-wakeup event adapter.
usage: core-player <URI> [plugin-directory]. output uses saved settings. */
#include <musikcore/musikcore.h>
#include "host-wakeup.h"
#include <stdio.h>
#include <string.h>

struct playback_state {
    bool done;
    bool failed;
};

/* foreign callbacks run on the dispatching thread. never destroy this player
inside its callback; record completion and close it after dispatch returns. */
static void MC_CALL changed(void* data, const mc_player_snapshot* snapshot) {
    struct playback_state* state = data;
    state->failed = snapshot->state == MC_PLAYER_FAILED;
    state->done = state->failed || snapshot->state == MC_PLAYER_FINISHED;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fputs("usage: core-player <URI> [plugin-directory]\n", stderr);
        return 1;
    }
    test_host_event* event = test_host_event_create();
    if (!event) {
        return 1;
    }
    mc_context* context = NULL;
    mc_player* player = NULL;
    struct playback_state state = { false, false };
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    /* Plugins supplies streams/decoders/output; no library or indexer is opened.
    the context owns provider lifetime and dispatch, while we own the player. */
    options.services = MC_SERVICE_PLUGINS;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = event;
    if (argc == 3) {
        options.plugin_directory = (mc_string_view) { argv[2], strlen(argv[2]) };
    }
    if (mc_context_create(&options, &context, NULL) == MC_OK &&
        mc_player_create(context, &player) == MC_OK &&
        mc_player_set_callback(player, changed, &state) == MC_OK &&
        mc_player_play_uri(player, (mc_string_view) { argv[1], strlen(argv[1]) }) == MC_OK) {
        /* a long track may emit no changes for minutes; wait for the event
        rather than treating the test adapter's short default timeout as EOF. */
        while (!state.done && test_host_event_wait_for(event, UINT32_MAX)) {
            if (mc_context_dispatch(context) != MC_OK) {
                break;
            }
        }
    }
    /* closing the owned player interrupts opens and waits for its workers.
    keep callback state and the event alive through context shutdown as well. */
    const bool passed = state.done && !state.failed;
    const mc_result closed = mc_player_destroy(&player);
    const mc_result stopped = mc_context_destroy(&context);
    test_host_event_destroy(event);
    return passed && closed == MC_OK && stopped == MC_OK ? 0 : 1;
}
