#ifndef MUSIKCORE_PLUGIN_OUTPUT_H
#define MUSIKCORE_PLUGIN_OUTPUT_H

#include <musikcore/plugin/audio.h>
#include <musikcore/output.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_OUTPUT "org.musikcube.output"

typedef struct mc_output_vtable mc_output_vtable;

/* an audio output backend, such as PulseAudio, WASAPI, or CoreAudio.
musikcore calls api with instance to send PCM audio and control playback.
copying this struct does not create another output or keep it alive. */
typedef struct mc_output {
    /* readable bytes; initialize with MC_OUTPUT_DEFAULT. */
    uint32_t struct_size;

    /* opaque plugin-owned state, passed unchanged to api; default null. */
    void* instance;

    /* immutable plugin-owned methods, valid through destroy; default null.
    keep the plugin loaded until instance destruction finishes. */
    const mc_output_vtable* api;
} mc_output;

#define MC_OUTPUT_DEFAULT { sizeof(mc_output), NULL, NULL }

/* call completed when musikcore can reuse a buffer. call ready when musikcore
can retry a write. for example, copying a block into the last free device buffer can
release the original buffer before the device has room for another block.

both callbacks may run inside queue_audio_buffer or on a backend thread.
the host keeps context alive until stop returns. finish all callbacks by then.
callbacks must not block, allocate, throw, or call stop or destroy. */
typedef struct mc_output_callbacks {
    /* readable bytes; initialize with MC_OUTPUT_CALLBACKS_DEFAULT. */
    uint32_t struct_size;

    /* host-owned callback state; passed unchanged. default null. */
    void* context;

    /* return each accepted block's token exactly once, after the plugin has
    finished using its samples. the driver may still be playing its copy.
    required for playback. default null. */
    void(MC_CALL* completed)(void* context, uint64_t token, mc_result outcome);

    /* after MC_ERROR_QUEUE_FULL, call ready whenever a retry could succeed
    or report a terminal error. make the space or error visible to the next
    write before calling ready, even if this happens inside queue_audio_buffer.
    one call can cover several freed buffers. musikcore checks again on retry.
    default null. an output that returns MC_ERROR_QUEUE_FULL requires this
    callback. musikcore always supplies it. */
    void(MC_CALL* ready)(void* context);
} mc_output_callbacks;

#define MC_OUTPUT_CALLBACKS_DEFAULT { sizeof(mc_output_callbacks), NULL, NULL, NULL }

/* tells an output plugin which device and audio format to use. musikcore passes
these settings to prepare before it starts sending audio blocks. */
typedef struct mc_output_options {
    /* readable bytes; initialize with MC_OUTPUT_OPTIONS_DEFAULT. */
    uint32_t struct_size;

    /* negotiated PCM format, copied during prepare; default unset. */
    mc_audio_format format;

    /* non-owning UTF-8 device ID, copied during prepare; empty selects default. */
    mc_string_view device_id;

    /* maximum queued frames; zero selects the backend's bounded default. */
    size_t queue_capacity_frames;

    /* initial normalized volume in [0, 1]; default 1. */
    double initial_volume;
} mc_output_options;

#define MC_OUTPUT_OPTIONS_DEFAULT { sizeof(mc_output_options), MC_AUDIO_FORMAT_DEFAULT, { NULL, 0 }, 0, 1.0 }

/* an audio block musikcore asks an output plugin to play. token identifies that
block when the plugin reports completion, so musikcore knows which buffer to reuse. */
typedef struct mc_output_buffer {
    /* readable bytes; initialize with MC_OUTPUT_BUFFER_DEFAULT. */
    uint32_t struct_size;

    /* nonzero host token, unique until returned by completed; default zero. */
    uint64_t token;

    /* read-only PCM. backing storage stays alive until completed, including
    completion inline in queue_audio_buffer. the default empty view is not submittable. */
    mc_pcm_view pcm;
} mc_output_buffer;

#define MC_OUTPUT_BUFFER_DEFAULT { sizeof(mc_output_buffer), 0, MC_PCM_VIEW_DEFAULT }

typedef uint32_t mc_output_timing_flags;
enum {
    MC_OUTPUT_TIMING_PLAYED = 1u << 0,
    MC_OUTPUT_TIMING_QUEUED = 1u << 1,
    MC_OUTPUT_TIMING_DEVICE_LATENCY = 1u << 2,
    MC_OUTPUT_TIMING_ESTIMATED = 1u << 3
};

/* reports how far an output backend has played and how much audio is still
waiting. musikcore can use this to align playback position and visualizations
with sound leaving the device; validity flags identify what the backend knows. */
typedef struct mc_output_timing {
    /* writable bytes supplied by caller; initialize with MC_OUTPUT_TIMING_DEFAULT. */
    uint32_t struct_size;

    /* MC_OUTPUT_TIMING_* bits distinguish unknown values and estimates; default zero. */
    mc_output_timing_flags valid_flags;

    /* changes on prepare/reset; compare counts only within a generation. default zero. */
    uint64_t generation;

    /* total PCM frames the driver reports as played in this generation, not
    frames merely queued. meaningful with MC_OUTPUT_TIMING_PLAYED; default zero.
    one frame includes every channel. mark estimates with MC_OUTPUT_TIMING_ESTIMATED. */
    uint64_t frames_played_by_driver;

    /* host monotonic-clock time associated with frames_played_by_driver, in
    nanoseconds; not the time a cached reading was retrieved. meaningful with
    MC_OUTPUT_TIMING_PLAYED; default zero. */
    uint64_t frames_played_timestamp_ns;

    /* accepted frames awaiting consumption, in seconds;
    meaningful with MC_OUTPUT_TIMING_QUEUED. */
    double queued_seconds;

    /* additional device delay in seconds; meaningful with MC_OUTPUT_TIMING_DEVICE_LATENCY.
    defaults to zero without claiming a measurement. */
    double device_latency_seconds;
} mc_output_timing;

#define MC_OUTPUT_TIMING_DEFAULT { sizeof(mc_output_timing), 0, 0, 0, 0, 0.0, 0.0 }

/* functions an output plugin implements to play PCM audio through its backend.
musikcore uses them to prepare the device, send audio, and control playback.
ordinary calls are caller-serialized; stop may overlap queue_audio_buffer or
drain, and get_timing may overlap any call. tables are immutable and every
method is required. default method pointers are null.

the host validates argument pointers, struct prefixes (including nested views),
PCM counts/storage, formats, normalized volume, and required callbacks before
calling. it queues only after successful prepare, stops before reprepare, and
stops/joins all users before destroy. plugins may trust these preconditions;
backend/device availability, capacity, and operational failures remain theirs. */
struct mc_output_vtable {
    /* readable bytes; initialize with MC_OUTPUT_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* release plugin-owned state. the host has successfully stopped the
    instance and finished all calls/callbacks before invoking destroy. */
    void(MC_CALL* destroy)(void* instance);

    /* worker/control call on an unopened or stopped instance. copy settings
    and open the backend. the host stops partial state after failure and does
    not submit audio unless prepare succeeded. */
    mc_result(MC_CALL* prepare)(
        void* instance, const mc_output_options* options, const mc_output_callbacks* callbacks, mc_error** error);

    /* accept one block for playback. musikcore calls this on its producer
    worker. the call may block while the device accepts or plays the block.
    stop must interrupt a blocked call and cancel any wait for space.

    return MC_OK if the plugin accepted the block. musikcore keeps its samples
    alive until completed returns the token. if stop later discards the block,
    return its token through completed with MC_ERROR_CANCELLED.

    return MC_ERROR_QUEUE_FULL if the block fits but the queue is full for now.
    use ready to wake musikcore for another try. return MC_ERROR_BUFFER_TOO_SMALL
    if the block can never fit at the current capacity. the caller can then split
    it or prepare a larger queue. on any error, leave the block with musikcore
    and do not call completed for it. */
    mc_result(MC_CALL* queue_audio_buffer)(void* instance, const mc_output_buffer* buffer);

    /* freeze frame consumption; buffers may still fill available capacity. */
    mc_result(MC_CALL* pause)(void* instance);

    /* continue consumption without counting time spent paused. */
    mc_result(MC_CALL* resume)(void* instance);

    /* control call: set normalized volume in [0, 1]. */
    mc_result(MC_CALL* set_volume)(void* instance, double normalized);

    /* control call: copy current normalized volume to caller storage. */
    mc_result(MC_CALL* get_volume)(void* instance, double* normalized);

    /* discard pending audio, return all accepted tokens, cancel/wake drain,
    interrupt blocked queue_audio_buffer/drain calls, and close admission until
    prepare. no callbacks remain after return. join producer calls before destroy. */
    mc_result(MC_CALL* stop)(void* instance);

    /* worker call: wait for accepted audio. stop wakes it with cancellation.
    paused output with pending PCM reports MC_ERROR_IN_FLIGHT instead of waiting forever. */
    mc_result(MC_CALL* drain)(void* instance, mc_error** error);

    /* bounded concurrent snapshot; no allocation, blocking, or error object.
    validity bits describe available estimates or device measurements. */
    mc_result(MC_CALL* get_timing)(void* instance, mc_output_timing* timing);

    /* preferred format for this instance's device; callable before prepare.
    empty ID selects the system default. */
    mc_result(MC_CALL* get_default_format)(void* instance, mc_string_view device_id, mc_audio_format* format);
};

#define MC_OUTPUT_VTABLE_DEFAULT                                                                                       \
    { sizeof(mc_output_vtable), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

/* device strings and records are callback-scoped; copy to retain them. */
typedef void(MC_CALL* mc_output_device_fn)(void* user_data, const mc_output_device* device);

/* lets musikcore discover a backend's devices and create an output for playback.
for example, a CoreAudio plugin supplies this table to enumerate macOS devices
and create mc_output instances. factory calls may run on concurrent workers;
create returns an unopened instance, and prepare opens its device. */
typedef struct mc_output_factory {
    /* readable bytes; initialize with MC_OUTPUT_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned factory state through plugin shutdown; default null. */
    void* context;

    /* immutable table validated before create, alive through destroy; default null. */
    const mc_output_vtable* instance_api;

    /* enumerate device snapshots through read, consumed before callback return. */
    mc_result(MC_CALL* visit_devices)(void* context, mc_output_device_fn read, void* user_data);

    /* report the default device through read, or MC_ERROR_NOT_FOUND when unavailable. */
    mc_result(MC_CALL* read_default_device)(void* context, mc_output_device_fn read, void* user_data);

    /* transfer instance ownership to out_instance, initially null. a nonnull
    partial instance returned on failure must support stop; the host successfully
    stops it before destroy. services include plugin-scoped preferences and stay
    host-owned through destruction;
    error is optional/host-owned. */
    mc_result(MC_CALL* create)(
        void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);
} mc_output_factory;

#define MC_OUTPUT_FACTORY_DEFAULT { sizeof(mc_output_factory), NULL, NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
