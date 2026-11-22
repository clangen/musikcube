#include "error.h"

#include <string>

struct mc_error {
    mc_result code;
    std::string message;
};

namespace {
    const char* completionReason(mc_result result) noexcept {
        switch (result) {
            case MC_ERROR_INVALID_ARGUMENT:
                return "An argument is invalid";
            case MC_ERROR_NOT_FOUND:
                return "The requested item was not found";
            case MC_ERROR_UNSUPPORTED:
                return "The requested operation is not supported";
            case MC_ERROR_BUSY:
                return "The service is busy";
            case MC_ERROR_CANCELLED:
                return "The operation was cancelled";
            case MC_ERROR_IO:
                return "A read, write, or connection operation failed";
            case MC_ERROR_PLUGIN:
                return "A required plugin could not complete the operation";
            case MC_ERROR_INTERNAL:
                return "An internal error prevented the operation";
            case MC_ERROR_BUFFER_TOO_SMALL:
                return "The output buffer is too small";
            case MC_ERROR_QUEUE_FULL:
                return "The context request queue is full";
            case MC_ERROR_STALE_SNAPSHOT:
                return "The data changed. Refresh before retrying";
            case MC_ERROR_IN_FLIGHT:
                return "The operation is already in progress";
            case MC_ERROR_OUTCOME_UNKNOWN:
                return "The backend may have applied the mutation. Refresh before retrying";
            case MC_ERROR_WRONG_THREAD:
                return "The operation was called from the wrong thread";
            default:
                return "An unknown error prevented the operation";
        }
    }
}

namespace musik { namespace core { namespace ffi {

    CompletionError::CompletionError(mc_result result, const char* operation, const char* detail) noexcept {
        if (result == MC_OK) {
            return;
        }
        try {
            std::string message;
            if (operation && *operation) {
                message.append(operation).append(": ");
            }
            const bool warning = result == MC_ERROR_QUEUE_FULL || result == MC_ERROR_OUTCOME_UNKNOWN;
            if (detail && *detail && !warning) {
                message.append(detail);
            }
            else {
                message.append(completionReason(result));
                if (detail && *detail) {
                    message.append(". ").append(detail);
                }
            }
            fail(result, message.c_str(), &this->error);
        }
        catch (...) {
            /* diagnostics are optional; allocation failure must not escape or
            replace the original completion status. */
        }
    }

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
