#include <musikcore/musikcore.h>
#include "host-wakeup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct completion {
    bool done;
    mc_result status;
};

static bool await(test_host_event* event, mc_context* context, const struct completion* completion) {
    while (!completion->done && test_host_event_wait(event)) {
        if (mc_context_dispatch(context) != MC_OK) {
            return false;
        }
    }
    return completion->done && completion->status == MC_OK;
}

static void MC_CALL connected(mc_result result, mc_remote_state state, const mc_error* error, void* data) {
    struct completion* completion = data;
    completion->status = state == MC_REMOTE_CONNECTED ? result : MC_ERROR_IO;
    completion->done = true;
}

static void MC_CALL searched(mc_result result, mc_track_result* tracks, const mc_error* error, void* data) {
    struct completion* completion = data;
    if (result == MC_OK) {
        printf("%zu tracks\n", mc_track_result_get_count(tracks));
    }
    mc_track_result_destroy(&tracks);
    completion->status = result;
    completion->done = true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fputs("Usage: musikcore_example_remote host websocket-port\n", stderr);
        return 1;
    }

    char* end = NULL;
    const unsigned long port = strtoul(argv[2], &end, 10);
    if (!port || port > UINT16_MAX || *end) {
        return 1;
    }

    test_host_event* event = test_host_event_create();
    if (!event) {
        return 1;
    }

    mc_context_options host = MC_CONTEXT_OPTIONS_DEFAULT;
    host.wakeup = test_host_event_signal;
    host.wakeup_user_data = event;
    mc_remote_options endpoint = MC_REMOTE_OPTIONS_DEFAULT;
    endpoint.host = (mc_string_view) { argv[1], strlen(argv[1]) };
    endpoint.websocket_port = (uint16_t) port;
    const char* password = getenv("MUSIKCORE_REMOTE_PASSWORD");
    if (password) {
        endpoint.password = (mc_string_view) { password, strlen(password) };
    }

    mc_context* context = NULL;
    mc_remote_session* session = NULL;
    mc_library* library = NULL;
    mc_request_id request = 0;
    const mc_track_search search = MC_TRACK_SEARCH_DEFAULT;
    struct completion connection = { false, MC_ERROR_INTERNAL };
    struct completion results = { false, MC_ERROR_INTERNAL };
    const bool passed =
        mc_context_create(&host, &context, NULL) == MC_OK &&
        mc_remote_session_create(&endpoint, &session) == MC_OK &&
        mc_remote_session_connect_async(context, session, connected, &connection, &request) == MC_OK &&
        await(event, context, &connection) &&
        mc_remote_session_open_library(session, &library) == MC_OK &&
        mc_library_open_track_search_async(context, library, &search, searched, &results, &request) == MC_OK &&
        await(event, context, &results);

    mc_library_destroy(&library);
    mc_remote_session_destroy(&session);
    mc_context_destroy(&context);
    test_host_event_destroy(event);
    return passed ? 0 : 1;
}
