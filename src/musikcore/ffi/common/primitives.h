#ifndef MUSIKCORE_FFI_COMMON_PRIMITIVES_H
#define MUSIKCORE_FFI_COMMON_PRIMITIVES_H

#include <musikcore/abi.h>

namespace musik { namespace core { namespace ffi {

    /* Query capacity with a null destination and zero capacity. Short buffers are left intact. */
    mc_result copy_string(mc_string_view source, char *destination, size_t capacity, size_t *required);
    mc_result copy_bytes(mc_byte_view source, uint8_t *destination, size_t capacity, size_t *required);

    template <typename T>
    T init_sized() {
        static_assert(offsetof(T, struct_size) == 0, "struct_size must be first");
        static_assert(sizeof(T) <= UINT32_MAX, "structure exceeds ABI size limit");
        T value{};
        value.struct_size = static_cast<uint32_t>(sizeof(T));
        return value;
    }

    inline bool has_field(uint32_t struct_size, size_t offset, size_t width) {
        return offset <= struct_size && width <= struct_size - offset;
    }

} } }

#endif
