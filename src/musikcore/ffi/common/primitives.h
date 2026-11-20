#ifndef MUSIKCORE_FFI_COMMON_PRIMITIVES_H
#define MUSIKCORE_FFI_COMMON_PRIMITIVES_H

#include <musikcore/abi.h>
#include <cstring>

namespace musik { namespace core { namespace ffi {

    /* Validate lengths before inspecting caller storage or constructing containers. */
    inline bool valid_text(mc_string_view text, bool allow_empty = true) {
        return text.length <= static_cast<size_t>(PTRDIFF_MAX) &&
            (text.length ? text.data && !std::memchr(text.data, '\0', text.length) : allow_empty);
    }

    inline bool valid_ids(const int64_t* ids, size_t count) {
        if ((count && !ids) || count > static_cast<size_t>(PTRDIFF_MAX) / sizeof(*ids)) return false;
        for (size_t i = 0; i < count; ++i) if (ids[i] <= 0) return false;
        return true;
    }

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

    template <typename Struct, typename T>
    T read_option(const Struct *options, T Struct::*field, size_t offset) {
        return options && has_field(options->struct_size, offset, sizeof(T)) ? options->*field : T{};
    }

} } }

#endif
