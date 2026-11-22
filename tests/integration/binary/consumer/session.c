#include <musikcore/musikcore.h>

#include "host-wakeup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Installed session contract failed at line %d: %s\n", __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

typedef struct host {
    mc_context* context;
    test_host_event* event;
    const void* owner;
    unsigned notifications;
    bool callback_contract;
    bool rejected_off_thread;
} host;

typedef struct reply {
    const host* host;
    unsigned calls;
    bool on_owner;
    bool notification_delivered;
    mc_result status;
    mc_track_result* tracks;
    mc_track_page* page;
} reply;

static mc_string_view text(const char* value) {
    return (mc_string_view) { value, strlen(value) };
}

static void MC_CALL connected(mc_result status, mc_remote_state state, const mc_error* error, void* data) {
    (void) error;
    reply* result = data;
    result->calls++;
    result->on_owner = result->host->owner == test_host_thread_token();
    result->status = state == MC_REMOTE_CONNECTED ? status : MC_ERROR_IO;
}

static void MC_CALL library_changed(void* data, bool selection_changed) {
    host* state = data;
    mc_context* original = state->context;

    state->notifications++;
    state->callback_contract = state->owner == test_host_thread_token() && selection_changed &&
        mc_context_set_library_changed_callback(original, NULL, NULL) == MC_OK &&
        mc_context_dispatch(original) == MC_ERROR_BUSY &&
        mc_context_shutdown(original) == MC_ERROR_BUSY &&
        mc_context_destroy(&state->context) == MC_ERROR_BUSY && state->context == original;
}

static void MC_CALL search_done(mc_result status, mc_track_result* tracks, const mc_error* error, void* data) {
    (void) error;
    reply* result = data;
    result->calls++;
    result->on_owner = result->host->owner == test_host_thread_token();
    result->notification_delivered = result->host->notifications == 1;
    result->status = status;
    result->tracks = tracks;
}

static void MC_CALL read_done(mc_result status, mc_track_page* page, const mc_error* error, void* data) {
    (void) error;
    reply* result = data;
    result->calls++;
    result->on_owner = result->host->owner == test_host_thread_token();
    result->status = status;
    result->page = page;
}

static bool wait_reply(const host* state, const reply* result) {
    while (!result->calls) {
        if (mc_context_dispatch(state->context) != MC_OK) {
            return false;
        }
        if (!result->calls && !test_host_event_wait(state->event)) {
            return false;
        }
    }
    return result->calls == 1 && result->on_owner && result->status == MC_OK;
}

static void wrong_thread(void* data) {
    host* state = data;
    state->rejected_off_thread = mc_context_dispatch(state->context) == MC_ERROR_WRONG_THREAD &&
        mc_context_shutdown(state->context) == MC_ERROR_WRONG_THREAD;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        return 1;
    }

    char* end = NULL;
    const unsigned long port = strtoul(argv[2], &end, 10);
    if (*end || !port || port > UINT16_MAX) {
        return 1;
    }

    int outcome = 1;
    host state = { .owner = test_host_thread_token(), .event = test_host_event_create() };
    reply connection = { .host = &state }, remote = { .host = &state }, local = { .host = &state };
    reply remote_row = { .host = &state }, local_row = { .host = &state }, pending = { .host = &state };
    mc_library* local_library = NULL;
    mc_library* remote_library = NULL;
    mc_remote_session* session = NULL;
    mc_track_ref* track = NULL;
    mc_request_id request = 0;

    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_NONE;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = state.event;
    CHECK(state.event && mc_context_create(&options, &state.context, NULL) == MC_OK);
    CHECK(mc_context_dispatch(state.context) == MC_OK);
    CHECK(test_host_run_worker(wrong_thread, &state) && state.rejected_off_thread);

    mc_library_options local_options = MC_LIBRARY_OPTIONS_DEFAULT;
    local_options.locator = text(argv[1]);
    CHECK(mc_library_create(&local_options, &local_library) == MC_OK);

    mc_remote_options remote_options = MC_REMOTE_OPTIONS_DEFAULT;
    remote_options.host = text("127.0.0.1");
    remote_options.websocket_port = (uint16_t) port;
    CHECK(mc_remote_session_create(&remote_options, &session) == MC_OK);
    CHECK(mc_remote_session_connect_async(state.context, session, connected, &connection, &request) == MC_OK);
    CHECK(wait_reply(&state, &connection));
    CHECK(mc_remote_session_open_library(session, &remote_library) == MC_OK);
    CHECK(mc_context_set_library(state.context, remote_library) == MC_OK);
    mc_library_destroy(&remote_library);

    /* The fixture withholds this reply until the client selects the local catalog. */
    CHECK(mc_context_set_library_changed_callback(state.context, library_changed, &state) == MC_OK);
    mc_track_search query = MC_TRACK_SEARCH_DEFAULT;
    query.text = text("Held remote search");
    CHECK(mc_library_open_track_search_async(state.context, NULL, &query, search_done, &remote, &request) == MC_OK);
    CHECK(mc_context_set_library(state.context, local_library) == MC_OK);
    mc_library_destroy(&local_library);
    query.text = text("Morning Light");
    CHECK(mc_library_open_track_search_async(state.context, NULL, &query, search_done, &local, &request) == MC_OK);
    CHECK(puts("release") >= 0 && fflush(stdout) == 0);

    CHECK(wait_reply(&state, &remote) && wait_reply(&state, &local));
    CHECK(state.callback_contract && remote.notification_delivered && local.notification_delivered);
    CHECK(mc_track_result_get_count(remote.tracks) == 2 && mc_track_result_get_count(local.tracks) == 1);

    /* Resolve the remote row after dropping its result and the original library handle. */
    CHECK(mc_track_result_get_ref(remote.tracks, 0, &track) == MC_OK);
    mc_track_result_destroy(&remote.tracks);
    CHECK(mc_track_ref_read_async(state.context, track, NULL, 0, read_done, &remote_row, &request) == MC_OK);
    mc_track_ref_destroy(&track);
    CHECK(wait_reply(&state, &remote_row));
    const mc_string_view origin = mc_track_page_get_string(remote_row.page, 0, 0);
    CHECK(origin.length == sizeof("Origin") - 1 && memcmp(origin.data, "Origin", origin.length) == 0);

    mc_track_range range = MC_TRACK_RANGE_DEFAULT;
    CHECK(mc_track_result_read_async(state.context, local.tracks, &range, read_done, &local_row, &request) == MC_OK);
    mc_track_result_destroy(&local.tracks);
    CHECK(wait_reply(&state, &local_row));
    const mc_string_view title = mc_track_page_get_string(local_row.page, 0, 0);
    CHECK(title.length == sizeof("Morning Light") - 1 && memcmp(title.data, "Morning Light", title.length) == 0);

    /* A second selection change must not invoke the self-unsubscribed observer. */
    CHECK(mc_remote_session_open_library(session, &remote_library) == MC_OK);
    CHECK(mc_context_set_library(state.context, remote_library) == MC_OK);
    CHECK(mc_context_dispatch(state.context) == MC_OK && state.notifications == 1);

    /* Shut down on the owner thread with a remote reply still withheld. */
    query.text = text("Held remote search");
    CHECK(mc_library_open_track_search_async(state.context, NULL, &query, search_done, &pending, &request) == MC_OK);
    CHECK(mc_context_shutdown(state.context) == MC_OK);
    CHECK(pending.calls == 1 && pending.on_owner && pending.status == MC_ERROR_CANCELLED && !pending.tracks);
    CHECK(mc_remote_session_state(session) == MC_REMOTE_CONNECTED);
    CHECK(mc_context_destroy(&state.context) == MC_OK && !state.context);
    CHECK(connection.calls == 1 && remote.calls == 1 && local.calls == 1 &&
        remote_row.calls == 1 && local_row.calls == 1 && pending.calls == 1);
    outcome = 0;

cleanup:
    mc_context_destroy(&state.context);
    mc_remote_session_destroy(&session);
    mc_library_destroy(&local_library);
    mc_library_destroy(&remote_library);
    mc_track_ref_destroy(&track);
    mc_track_result_destroy(&pending.tracks);
    mc_track_result_destroy(&remote.tracks);
    mc_track_result_destroy(&local.tracks);
    mc_track_page_destroy(&remote_row.page);
    mc_track_page_destroy(&local_row.page);
    if (state.event) {
        test_host_event_destroy(state.event);
    }
    return outcome;
}
