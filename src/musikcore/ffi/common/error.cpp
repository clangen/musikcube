#include "error.h"

#include <string>

struct mc_error {
    mc_result code;
    std::string message;
};

namespace musik { namespace core { namespace ffi {

    mc_result fail(mc_result code, const char *message, mc_error **out_error) noexcept {
        if (out_error) {
            try {
                *out_error = new mc_error{ code, message };
            } catch (...) {
                /* Preserve the original result when error details cannot be allocated. */
                *out_error = nullptr;
            }
        }
        return code;
    }

} } }

mc_result MC_CALL mc_error_get_code(const mc_error *error) {
    return error ? error->code : MC_ERROR_INVALID_ARGUMENT;
}

mc_string_view MC_CALL mc_error_get_message(const mc_error *error) {
    return error ? mc_string_view{ error->message.data(), error->message.size() } : mc_string_view{};
}

void MC_CALL mc_error_destroy(mc_error **error) {
    if (!error) return;
    delete *error;
    *error = nullptr;
}
