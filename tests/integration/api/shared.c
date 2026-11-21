#include <musikcore/musikcore.h>

#include "../../support/check.h"
#include <stdio.h>

_Static_assert(sizeof(mc_library_kind) == 4, "library kind width");
_Static_assert(sizeof(mc_track_search_mode) == 4, "search mode width");
_Static_assert(sizeof(mc_track_search_order) == 4, "search order width");

struct event_observer {
    mc_subscription *subscription;
    int total;
};

static void MC_CALL observe_event(void *user_data, const void *event) {
    struct event_observer *observer = user_data;
    observer->total += *(const int *) event;
}

static void MC_CALL observe_once(void *user_data, const void *event) {
    struct event_observer *observer = user_data;
    observe_event(user_data, event);
    mc_subscription_destroy(&observer->subscription);
}

static int check_event_subscriptions(void) {
    mc_event_source *source = NULL;
    struct event_observer once = { NULL, 0 };
    struct event_observer repeat = { NULL, 0 };
    const int event = 7;
    int passed = 0;

    if (!TEST_CHECK(mc_event_source_create(&source) == MC_OK) ||
        !TEST_CHECK(mc_event_source_subscribe(source, observe_once, &once, &once.subscription) == MC_OK) ||
        !TEST_CHECK(mc_event_source_subscribe(source, observe_event, &repeat, &repeat.subscription) == MC_OK) ||
        !TEST_CHECK(mc_event_source_subscribe(source, observe_event, &repeat, &repeat.subscription) ==
            MC_ERROR_INVALID_ARGUMENT) ||
        !TEST_CHECK(mc_event_source_publish(source, &event) == MC_OK) ||
        !TEST_CHECK(!once.subscription) ||
        !TEST_CHECK(mc_event_source_publish(source, &event) == MC_OK)) {
        goto cleanup;
    }

    mc_event_source_destroy(&source);
    passed = TEST_CHECK(!source && once.total == event && repeat.total == event * 2);

cleanup:
    mc_subscription_destroy(&once.subscription);
    mc_subscription_destroy(&repeat.subscription);
    mc_event_source_destroy(&source);
    return passed && TEST_CHECK(!repeat.subscription);
}

/* Pass occupied output slots to catch leaked/replaced owners, then release the library to expose dangling
   references in derived identities. */
static int check_owned_outputs(void) {
    mc_library_options options = MC_LIBRARY_OPTIONS_DEFAULT;
    options.locator = (mc_string_view){ ".", 1 };
    mc_library* library = NULL;
    mc_track_result* tracks = NULL;
    mc_track_ref* track = NULL;
    const int64_t ids[] = { 42, 17, 42 };
    const int64_t invalid[] = { 0 };
    int passed = 0;
    /* Creation must never replace a live owner, since doing so would lose its
       only release path even if the returned status reports an argument error. */
    if (mc_library_create(&options, &library) != MC_OK) goto cleanup;
    mc_library* original_library = library;
    if (mc_library_create(&options, &library) != MC_ERROR_INVALID_ARGUMENT || library != original_library) goto cleanup;
    if (mc_library_make_track_result(library, invalid, 1, &tracks) != MC_ERROR_INVALID_ARGUMENT || tracks) goto cleanup;
    if (mc_library_make_track_result(library, ids, 3, &tracks) != MC_OK) goto cleanup;
    mc_track_result* original_tracks = tracks;
    if (mc_library_make_track_result(library, ids, 3, &tracks) != MC_ERROR_INVALID_ARGUMENT || tracks != original_tracks) goto cleanup;
    if (mc_track_result_get_ref(tracks, 0, &track) != MC_OK) goto cleanup;
    mc_track_ref* original_track = track;
    if (mc_track_result_get_ref(tracks, 1, &track) != MC_ERROR_INVALID_ARGUMENT || track != original_track) goto cleanup;
    /* Derived identities must retain their origin rather than reference the caller's
       library wrapper, which can disappear before playback or another read. */
    mc_library_destroy(&library);
    passed = mc_track_result_get_count(tracks) == 3 && mc_track_result_get_id(tracks, 2) == 42 &&
        mc_track_ref_get_id(track) == 42 && mc_track_result_get_id(tracks, SIZE_MAX) == 0;
cleanup:
    mc_track_ref_destroy(&track);
    mc_track_result_destroy(&tracks);
    mc_library_destroy(&library);
    return passed;
}

/* Verify version negotiation, sized-option defaults, singleton admission, and repeatable teardown. */
static int check_context_lifecycle(void) {
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_NONE;
    mc_context *context = NULL;
    mc_context *second = NULL;
    mc_error *error = NULL;
    int passed = 0;

    /* Null/occupied output rejection must not publish or overwrite a context. */
    if (mc_context_create(NULL, NULL, NULL) != MC_ERROR_INVALID_ARGUMENT) goto cleanup;
    if (mc_context_create(&options, &context, &error) != MC_OK || !context || error) goto cleanup;
    {
        mc_context *original = context;
        if (mc_context_create(NULL, &context, NULL) != MC_ERROR_INVALID_ARGUMENT || context != original) goto cleanup;
    }
    mc_context_destroy(&context);

    /* A malformed options prefix must fail before any field is read; an
       occupied error slot must also preserve the caller's existing owner. */
    options.struct_size = 0;
    if (mc_context_create(&options, &context, &error) != MC_ERROR_INVALID_ARGUMENT ||
        context || mc_error_get_code(error) != MC_ERROR_INVALID_ARGUMENT) goto cleanup;
    {
        mc_error* original_error = error;
        if (mc_context_create(NULL, &context, &error) != MC_ERROR_INVALID_ARGUMENT ||
            error != original_error || context) goto cleanup;
    }
    mc_error_destroy(&error);

    const mc_context_options defaults = MC_CONTEXT_OPTIONS_DEFAULT;
    options = defaults;
    /* Both a different major and a future minor are unsupported contracts.
       Failed negotiation must leave the singleton available for a later call. */
    options.requested_abi_version = MC_ABI_VERSION_ENCODE(1, 0);
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_UNSUPPORTED || context) goto cleanup;
    options.requested_abi_version = MC_ABI_VERSION_ENCODE(0, MC_ABI_VERSION_MINOR(MC_ABI_VERSION) + 1);
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_UNSUPPORTED || context) goto cleanup;

    options.requested_abi_version = MC_ABI_VERSION;
    /* Reject a nonzero length with null storage before trying to copy it. */
    options.plugin_directory.length = 1;
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_INVALID_ARGUMENT || context) goto cleanup;

    /* omitted service selection defaults to a full engine, which this ABI-only
    fixture deliberately does not provide. failure must release the process slot. */
    options.struct_size = offsetof(mc_context_options, services);
    options.plugin_directory.length = 0;
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_UNSUPPORTED || context) goto cleanup;
    options.struct_size = sizeof(options);
    options.services = MC_SERVICE_NONE;
    if (mc_context_create(&options, &context, &error) != MC_OK || !context || error) goto cleanup;
    /* A second context cannot silently share process-global services. The
       original remains valid and releasing it makes admission possible again. */
    if (mc_context_create(NULL, &second, &error) != MC_ERROR_BUSY ||
        second || mc_error_get_code(error) != MC_ERROR_BUSY) goto cleanup;
    mc_error_destroy(&error);
    mc_context_destroy(&context);
    if (context) goto cleanup;

    if (mc_context_create(&options, &context, &error) != MC_OK || !context || error) goto cleanup;
    if (mc_context_dispatch(context) != MC_OK ||
        mc_request_cancel(context, 1) != MC_ERROR_NOT_FOUND ||
        mc_context_shutdown(context) != MC_OK) goto cleanup;
    mc_context_destroy(&context);
    mc_context_destroy(&context);
    mc_context_destroy(NULL);
    /* Repeated cycles catch a singleton flag or request state left behind by
       otherwise successful shutdown/destroy calls. */
    passed = !context;
    for (unsigned cycle = 0; passed && cycle < 100; ++cycle) {
        passed = mc_context_create(&options, &context, NULL) == MC_OK &&
            mc_context_dispatch(context) == MC_OK && mc_context_shutdown(context) == MC_OK;
        mc_context_destroy(&context);
        passed = passed && !context;
    }

cleanup:
    mc_error_destroy(&error);
    mc_context_destroy(&second);
    mc_context_destroy(&context);
    return passed;
}

/* Exercise a separately compiled C11 consumer through the actual shared-library boundary. */
int main(void) {
    if (!check_event_subscriptions()) {
        return 1;
    }

    if (mc_playback_control(NULL, MC_PLAYBACK_STOP, 0) != MC_ERROR_INVALID_ARGUMENT) return 1;
    mc_playback *playback = NULL;
    mc_playback_destroy(&playback);

    mc_error *error = NULL;
    const mc_string_view message = mc_error_get_message(error);
    if (mc_abi_version() != MC_ABI_VERSION ||
        mc_error_get_code(error) != MC_ERROR_INVALID_ARGUMENT ||
        message.data || message.length) {
        fputs("shared C ABI contract failed\n", stderr);
        return 1;
    }
    mc_error_destroy(&error);
    if (!TEST_CHECK(!error) || !TEST_CHECK(check_context_lifecycle()) || !TEST_CHECK(check_owned_outputs())) {
        fputs("shared C context lifecycle failed\n", stderr);
        return 1;
    }
    return 0;
}
