#include <musikcore/abi.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(mc_result) == 4, "result width");
_Static_assert(offsetof(mc_string_view, data) == 0, "string data offset");
_Static_assert(offsetof(mc_byte_view, data) == 0, "byte data offset");
_Static_assert(sizeof(mc_sample_format) == 4, "sample format width");
_Static_assert(sizeof(mc_audio_format) == 20, "audio format layout");
_Static_assert(offsetof(mc_audio_format, sample_format) == 12, "sample format offset");
_Static_assert(offsetof(mc_audio_buffer_view, struct_size) == 0, "buffer size offset");
_Static_assert(offsetof(mc_audio_buffer_view, frame_capacity) ==
    offsetof(mc_audio_buffer_view, frame_count) + sizeof(size_t), "buffer frame layout");
_Static_assert(MC_ABI_VERSION_ENCODE(1, 2) == UINT32_C(0x00010002), "version encoding");
_Static_assert(MC_ABI_VERSION_MAJOR(UINT32_C(0xabcd1234)) == UINT16_C(0xabcd), "major version");
_Static_assert(MC_ABI_VERSION_MINOR(UINT32_C(0xabcd1234)) == UINT16_C(0x1234), "minor version");

mc_result MC_CALL check_string_view(mc_string_view view);
int MC_CALL check_sized_structures(void);
mc_result MC_CALL check_error_operation(int mode, mc_error **error);

static int check_errors(void) {
    mc_error *error;
    if (check_error_operation(0, &error) != MC_OK || error) return 0;
    if (check_error_operation(1, NULL) != MC_ERROR_NOT_FOUND) return 0;

    mc_result result = check_error_operation(1, &error);
    const char expected[] = "Input is missing";
    const mc_string_view message = mc_error_get_message(error);
    const int valid_message = result == MC_ERROR_NOT_FOUND &&
        mc_error_get_code(error) == MC_ERROR_NOT_FOUND &&
        message.length == sizeof(expected) - 1 && message.data &&
        memcmp(message.data, expected, message.length) == 0;
    mc_error_destroy(&error);
    if (!valid_message || error) return 0;

    if (check_error_operation(2, &error) != MC_ERROR_INTERNAL || error) return 0;
    result = check_error_operation(4, &error);
    const int allocation_fallback = result == MC_ERROR_NOT_FOUND && !error;
    mc_error_destroy(&error);
    if (!allocation_fallback) return 0;

    result = check_error_operation(3, &error);
    const int translated = result == MC_ERROR_INTERNAL &&
        mc_error_get_code(error) == MC_ERROR_INTERNAL;
    mc_error_destroy(&error);
    mc_error_destroy(&error);
    mc_error_destroy(NULL);
    const mc_string_view empty = mc_error_get_message(NULL);
    return translated && !error && mc_error_get_code(NULL) == MC_ERROR_INVALID_ARGUMENT &&
        !empty.data && !empty.length;
}

int main(void) {
    const char data[] = { 'm', 'c' };
    const mc_string_view view = { data, sizeof(data) };
    if (mc_abi_version() != MC_ABI_VERSION) {
        fputs("ABI version mismatch\n", stderr);
        return 1;
    }
    if (check_string_view(view) != MC_OK) {
        fputs("caller-buffer contract failed\n", stderr);
        return 1;
    }
    if (!check_sized_structures()) {
        fputs("sized-structure contract failed\n", stderr);
        return 1;
    }
    if (!check_errors()) {
        fputs("error ownership or translation contract failed\n", stderr);
        return 1;
    }
    return 0;
}
