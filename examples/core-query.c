/* The host event adapter demonstrates integration with a sleeping event loop. */
#include <musikcore/musikcore.h>
#include "host-wakeup.h"
#include <stdio.h>
#include <string.h>

struct query_state { int done; mc_result result; };
static void MC_CALL completed(mc_result result, mc_track_page* page, const mc_error* error, void* data) {
    struct query_state* state = data;
    state->done = 1;
    state->result = result;
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
    if (argc != 2) return 1;
    test_host_event* event = test_host_event_create();
    if (!event) return 1;
    mc_context* context = NULL;
    mc_library* library = NULL;
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = event;
    mc_library_options library_options = MC_LIBRARY_OPTIONS_DEFAULT;
    library_options.locator = (mc_string_view){ argv[1], strlen(argv[1]) };
    mc_track_query query = MC_TRACK_QUERY_DEFAULT;
    mc_request_id request = 0;
    struct query_state state = { 0, MC_ERROR_INTERNAL };
    if (mc_context_create(&options, &context, NULL) == MC_OK &&
        mc_library_create(&library_options, &library) == MC_OK &&
        mc_library_query_tracks_async(context, library, &query, completed, &state, &request) == MC_OK) {
        while (!state.done && test_host_event_wait(event)) {
            if (mc_context_dispatch(context) != MC_OK) break;
        }
    }
    const int passed = state.done && state.result == MC_OK;
    mc_context_destroy(&context);
    mc_library_destroy(&library);
    test_host_event_destroy(event);
    return passed ? 0 : 1;
}
