#include "primitives.h"

#include <cstring>

namespace musik { namespace core { namespace ffi {

    namespace {

    mc_result copy_buffer(const void *source, size_t length, void *destination,
        size_t capacity, size_t trailing, size_t *required) {
        if (!required) return MC_ERROR_INVALID_ARGUMENT;
        *required = 0;
        if ((length && !source) || length > SIZE_MAX - trailing) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        if (!destination && capacity) return MC_ERROR_INVALID_ARGUMENT;

        *required = length + trailing;
        if (!destination) return MC_OK;
        if (capacity < *required) return MC_ERROR_BUFFER_TOO_SMALL;
        if (length) std::memmove(destination, source, length);
        if (trailing) static_cast<char *>(destination)[length] = '\0';
        return MC_OK;
    }

    }

    mc_result copy_string(mc_string_view source, char *destination, size_t capacity, size_t *required) {
        return copy_buffer(source.data, source.length, destination, capacity, 1, required);
    }

    mc_result copy_bytes(mc_byte_view source, uint8_t *destination, size_t capacity, size_t *required) {
        return copy_buffer(source.data, source.length, destination, capacity, 0, required);
    }

} } }
