#include <musikcore/abi.h>

#include <type_traits>
#include "primitives.h"

static_assert(std::is_same<mc_result, int32_t>::value, "result type");
static_assert(std::is_standard_layout<mc_string_view>::value, "string layout");
static_assert(std::is_standard_layout<mc_byte_view>::value, "byte layout");
static_assert(std::is_standard_layout<mc_audio_format>::value, "audio format layout");
static_assert(std::is_standard_layout<mc_audio_buffer_view>::value, "audio buffer view layout");
constexpr mc_audio_buffer_view default_audio_buffer = MC_AUDIO_BUFFER_VIEW_DEFAULT;
static_assert(default_audio_buffer.format.struct_size == sizeof(mc_audio_format), "nested format size");

extern "C" mc_result MC_CALL check_string_view(mc_string_view view) {
    using musik::core::ffi::copy_bytes;
    using musik::core::ffi::copy_string;
    size_t required = 0;
    char text[] = { 'x', 'x', 'x' };
    const uint8_t data[] = { 0, 255 };
    const mc_byte_view bytes = { data, sizeof(data) };
    uint8_t output[] = { 42, 42, 42 };

    if (copy_string(view, nullptr, 0, &required) != MC_OK || required != sizeof(text) ||
        copy_string(view, text, sizeof(text) - 1, &required) != MC_ERROR_BUFFER_TOO_SMALL ||
        required != sizeof(text) || text[0] != 'x' || text[1] != 'x' || text[2] != 'x' ||
        copy_string(view, text, sizeof(text), &required) != MC_OK ||
        text[0] != 'm' || text[1] != 'c' || text[2] != '\0' ||
        copy_string({ nullptr, 0 }, text, sizeof(text), &required) != MC_OK ||
        required != 1 || text[0] != '\0') {
        return MC_ERROR_INTERNAL;
    }
    if (copy_bytes(bytes, output, sizeof(output), &required) != MC_OK ||
        required != sizeof(data) || output[0] != 0 || output[1] != 255 || output[2] != 42 ||
        copy_string({ nullptr, 1 }, nullptr, 0, &required) != MC_ERROR_INVALID_ARGUMENT ||
        copy_string({ view.data, SIZE_MAX }, nullptr, 0, &required) != MC_ERROR_INVALID_ARGUMENT) {
        return MC_ERROR_INTERNAL;
    }
    return MC_OK;
}

extern "C" int MC_CALL check_sized_structures(void) {
    using musik::core::ffi::has_field;
    using musik::core::ffi::init_sized;
    struct Options {
        uint32_t struct_size;
        uint32_t flags;
        uint32_t added;
    };
    const auto current = init_sized<Options>();
    const auto old_size = static_cast<uint32_t>(offsetof(Options, added));
    return current.struct_size == sizeof(Options) && current.flags == 0 && current.added == 0 &&
        has_field(old_size, offsetof(Options, flags), sizeof(current.flags)) &&
        !has_field(old_size, offsetof(Options, added), sizeof(current.added)) &&
        has_field(current.struct_size, offsetof(Options, added), sizeof(current.added)) &&
        !has_field(current.struct_size, SIZE_MAX, 1);
}
