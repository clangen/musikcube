#ifndef MUSIKCORE_ABI_H
#define MUSIKCORE_ABI_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define MC_CALL __cdecl
#if defined(MC_BUILDING_SHARED)
#define MC_API __declspec(dllexport)
#elif defined(MC_USING_SHARED)
#define MC_API __declspec(dllimport)
#else
#define MC_API
#endif
#define MC_PLUGIN_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define MC_CALL
#define MC_API __attribute__((visibility("default")))
#define MC_PLUGIN_API __attribute__((visibility("default")))
#else
#define MC_CALL
#define MC_API
#define MC_PLUGIN_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ABI versions encode the major and minor numbers in 16 bits each. */
#define MC_ABI_VERSION_ENCODE(major, minor) \
    ((((uint32_t)(major) & UINT32_C(0xffff)) << 16) | ((uint32_t)(minor) & UINT32_C(0xffff)))
#define MC_ABI_VERSION_MAJOR(version) ((uint16_t)((uint32_t)(version) >> 16))
#define MC_ABI_VERSION_MINOR(version) ((uint16_t)(version))
#define MC_ABI_VERSION MC_ABI_VERSION_ENCODE(0, 3)

MC_API uint32_t MC_CALL mc_abi_version(void);

typedef int32_t mc_result;
enum {
    MC_OK = 0,
    MC_ERROR_INVALID_ARGUMENT = 1,
    MC_ERROR_NOT_FOUND = 2,
    MC_ERROR_UNSUPPORTED = 3,
    MC_ERROR_BUSY = 4,
    MC_ERROR_CANCELLED = 5,
    MC_ERROR_IO = 6,
    MC_ERROR_PLUGIN = 7,
    MC_ERROR_INTERNAL = 8,
    MC_ERROR_BUFFER_TOO_SMALL = 9,
    MC_ERROR_QUEUE_FULL = 10,
    MC_ERROR_STALE_SNAPSHOT = 11,
    MC_ERROR_IN_FLIGHT = 12,
    MC_ERROR_OUTCOME_UNKNOWN = 13
};

/* Non-owning UTF-8 view. Keep the backing storage alive while using it.
   Copy the bytes for longer use. The view itself needs no cleanup. */
typedef struct mc_string_view {
    /* Non-owning UTF-8 pointer; lifetime is set by the supplying API. Null is allowed for an empty view. */
    const char *data;
    /* Byte count, excluding any terminating NUL. */
    size_t length;
} mc_string_view;

/* Byte view that does not own or extend the lifetime of its storage. */
typedef struct mc_byte_view {
    /* Non-owning byte pointer; lifetime is set by the supplying API. Null is allowed for an empty view. */
    const uint8_t *data;
    /* Number of readable bytes. */
    size_t length;
} mc_byte_view;

typedef int32_t mc_sample_format;
enum {
    MC_SAMPLE_F32_NATIVE = 1
};

typedef struct mc_audio_format {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Sample frames per second. Zero means unset. */
    uint32_t sample_rate;
    /* Samples per interleaved frame. Zero means unset. */
    uint32_t channels;
    /* Encoding of each sample. Zero means unset. */
    mc_sample_format sample_format;
    /* Channel layout identifier. Zero means unspecified. */
    uint32_t channel_layout;
} mc_audio_format;

#define MC_AUDIO_FORMAT_DEFAULT { sizeof(mc_audio_format), 0, 0, 0, 0 }

/* Interleaved PCM view. It does not own or extend the lifetime of its storage. */
typedef struct mc_audio_buffer_view {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Non-owning PCM pointer. The owner must keep it valid during use. Null for the default empty view. */
    void *data;
    /* Number of populated frames. */
    size_t frame_count;
    /* Number of frames the storage can hold. */
    size_t frame_capacity;
    /* Format of the interleaved PCM frames. */
    mc_audio_format format;
} mc_audio_buffer_view;

#define MC_AUDIO_BUFFER_VIEW_DEFAULT { sizeof(mc_audio_buffer_view), NULL, 0, 0, MC_AUDIO_FORMAT_DEFAULT }

typedef struct mc_error mc_error;

/* Returns MC_ERROR_INVALID_ARGUMENT for a null handle. */
MC_API mc_result MC_CALL mc_error_get_code(const mc_error *error);
/* Error-owned message, valid until error destruction. A null handle returns an empty view. */
MC_API mc_string_view MC_CALL mc_error_get_message(const mc_error *error);
/* Consumes the handle and clears the caller's slot. Null is accepted. */
MC_API void MC_CALL mc_error_destroy(mc_error **error);

#ifdef __cplusplus
}
#endif

#endif
