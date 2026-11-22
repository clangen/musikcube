#include <musikcore/library.h>

#include "../common/primitives.h"

/* The SQLite-only fixture exports the complete ABI without a remote transport. */
mc_result MC_CALL mc_remote_session_create(const mc_remote_options* options, mc_remote_session** out_session)
{
    using musik::core::ffi::valid_text;
    if (!out_session || *out_session || !options || options->struct_size < sizeof(*options) ||
        !valid_text(options->host, false) || !valid_text(options->password))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return MC_ERROR_UNSUPPORTED;
}

void MC_CALL mc_remote_session_destroy(mc_remote_session**) {}

mc_remote_state MC_CALL mc_remote_session_state(const mc_remote_session*) {
    return MC_REMOTE_DISCONNECTED;
}

mc_result MC_CALL mc_remote_session_set_callback(mc_context*, mc_remote_session*,
    mc_remote_state_fn, void*)
{
    return MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_remote_session_connect_async(mc_context*, mc_remote_session*,
    mc_remote_connect_fn, void*, mc_request_id* out_request)
{
    if (out_request) {
        *out_request = 0;
    }
    return MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_remote_session_disconnect(mc_remote_session*) {
    return MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_remote_session_open_library(const mc_remote_session*, mc_library**) {
    return MC_ERROR_INVALID_ARGUMENT;
}
