/* a full engine owns configured catalogs, providers, indexing, and playback.
this example reads its selected local catalog without starting audible playback.
run with the desired HOME/XDG_CONFIG_HOME and a plugin directory argument. */
#include <musikcore/musikcore.h>
#include "host-wakeup.h"
#include <stdio.h>
#include <string.h>

struct query_state {
    bool done;
    mc_result status;
};

/* results transfer to the callback; strings remain valid until page release. */
static void MC_CALL completed(mc_result status, mc_track_page* page, const mc_error* error, void* data) {
    struct query_state* state = data;
    state->done = true;
    state->status = status;
    if (error) {
        const mc_string_view message = mc_error_get_message(error);
        fwrite(message.data, 1, message.length, stderr);
    }
    for (size_t row = 0; row < mc_track_page_get_count(page); ++row) {
        const mc_string_view title = mc_track_page_get_string(page, row, 0);
        fwrite(title.data, 1, title.length, stdout);
        fputc('\n', stdout);
    }
    mc_track_page_destroy(&page);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    test_host_event* event = test_host_event_create();
    if (!event) {
        return 1;
    }
    mc_context* context = NULL;
    mc_library* library = NULL;
    mc_playback* playback = NULL;
    struct query_state state = { false, MC_ERROR_INTERNAL };

    /* defaults request all engine services. choose a smaller immutable set with
    options.services, e.g. MC_SERVICE_PLAYBACK adds Library and Plugins, but
    leaves Indexer off. there is no manual prerequisite startup sequence. */
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.plugin_directory = (mc_string_view) { argv[1], strlen(argv[1]) };
    options.flags = MC_CONTEXT_START_LOCAL;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = event;
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;
    mc_request_id request = 0;
    if (mc_context_create(&options, &context, NULL) == MC_OK &&
        mc_context_get_library(context, &library) == MC_OK &&
        mc_context_get_playback(context, &playback) == MC_OK &&
        mc_library_query_tracks_async(context, library, &query, completed, &state, &request) == MC_OK) {
        /* wakeups signal readiness; only this creating thread dispatches work.
        GUI hosts would use their own event source instead of this adapter. */
        while (!state.done && test_host_event_wait(event)) {
            if (mc_context_dispatch(context) != MC_OK) {
                break;
            }
        }
    }

    /* shutdown settles accepted work while callback state/event are still alive.
    releasing a playback handle alone would not stop the engine. */
    const mc_result stopped = mc_context_destroy(&context);
    mc_playback_destroy(&playback);
    mc_library_destroy(&library);
    test_host_event_destroy(event);
    return state.done && state.status == MC_OK && stopped == MC_OK ? 0 : 1;
}
