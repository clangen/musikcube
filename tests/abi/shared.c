#include <musikcore/musikcore.h>

#include <stdio.h>

static int check_context_lifecycle(void) {
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    mc_context *context = NULL;
    mc_context *second = NULL;
    mc_error *error = NULL;
    int passed = 0;

    if (mc_context_create(NULL, NULL, NULL) != MC_ERROR_INVALID_ARGUMENT) goto cleanup;
    if (mc_context_create(&options, &context, &error) != MC_OK || !context || error) goto cleanup;
    mc_context_destroy(&context);

    options.struct_size = 0;
    if (mc_context_create(&options, &context, &error) != MC_ERROR_INVALID_ARGUMENT ||
        context || mc_error_get_code(error) != MC_ERROR_INVALID_ARGUMENT) goto cleanup;
    mc_error_destroy(&error);

    const mc_context_options defaults = MC_CONTEXT_OPTIONS_DEFAULT;
    options = defaults;
    options.requested_abi_version = MC_ABI_VERSION_ENCODE(1, 0);
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_UNSUPPORTED || context) goto cleanup;
    options.requested_abi_version = MC_ABI_VERSION_ENCODE(0, MC_ABI_VERSION_MINOR(MC_ABI_VERSION) + 1);
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_UNSUPPORTED || context) goto cleanup;

    options.requested_abi_version = MC_ABI_VERSION;
    options.data_directory.length = 1;
    if (mc_context_create(&options, &context, NULL) != MC_ERROR_INVALID_ARGUMENT || context) goto cleanup;

    /* Older callers omit fields past the reported size. */
    options.struct_size = offsetof(mc_context_options, data_directory);
    if (mc_context_create(&options, &context, &error) != MC_OK || !context || error) goto cleanup;
    if (mc_context_create(NULL, &second, &error) != MC_ERROR_BUSY ||
        second || mc_error_get_code(error) != MC_ERROR_BUSY) goto cleanup;
    mc_error_destroy(&error);
    mc_context_destroy(&context);
    if (context) goto cleanup;

    if (mc_context_create(NULL, &context, &error) != MC_OK || !context || error) goto cleanup;
    if (mc_context_dispatch(context) != MC_OK ||
        mc_request_cancel(context, 1) != MC_ERROR_NOT_FOUND ||
        mc_context_shutdown(context) != MC_OK) goto cleanup;
    mc_context_destroy(&context);
    mc_context_destroy(&context);
    mc_context_destroy(NULL);
    passed = !context;

cleanup:
    mc_error_destroy(&error);
    mc_context_destroy(&second);
    mc_context_destroy(&context);
    return passed;
}

int main(void) {
    mc_error *error = NULL;
    const mc_string_view message = mc_error_get_message(error);
    if (mc_abi_version() != MC_ABI_VERSION ||
        mc_error_get_code(error) != MC_ERROR_INVALID_ARGUMENT ||
        message.data || message.length) {
        fputs("shared C ABI contract failed\n", stderr);
        return 1;
    }
    mc_error_destroy(&error);
    if (error || !check_context_lifecycle()) {
        fputs("shared C context lifecycle failed\n", stderr);
        return 1;
    }
    return 0;
}
