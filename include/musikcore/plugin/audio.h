#ifndef MUSIKCORE_PLUGIN_AUDIO_H
#define MUSIKCORE_PLUGIN_AUDIO_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* growable decoder scratch storage, allocated by musikcore. decoders reuse it
through mc_audio_scratch_buffer_api; it is not a slice of the fixed playback pool. */
typedef struct mc_audio_scratch_buffer mc_audio_scratch_buffer;

/* host services available to one plugin instance, supplied when it is created. */
typedef struct mc_plugin_services mc_plugin_services;

/* lets long-running plugin work check whether the host wants it to stop,
without exposing the host's thread or cancellation implementation. */
typedef struct mc_cancel_token {
    /* readable bytes; initialize with MC_CANCEL_TOKEN_DEFAULT. */
    uint32_t struct_size;

    /* host-owned state, valid for the instance lifetime; default null. */
    void* context;

    /* thread-safe, allocation-free latched cancellation; nonzero means stop.
    pass context unchanged. a default token is unavailable and must not be called. */
    uint32_t(MC_CALL* is_cancelled)(void* context);
} mc_cancel_token;

#define MC_CANCEL_TOKEN_DEFAULT { sizeof(mc_cancel_token), NULL, NULL }

/* read-only audio passed to outputs, encoders, and visualizers without copying
samples. the supplier keeps storage alive; copying this view does not retain it. */
typedef struct mc_pcm_view {
    /* readable bytes; initialize with MC_PCM_VIEW_DEFAULT. */
    uint32_t struct_size;

    /* read-only interleaved native float32 samples. null only for an empty view.
    the supplier keeps frame_count * format.channels samples alive during use. */
    const float* data;

    /* number of complete interleaved frames, not individual samples; default zero. */
    size_t frame_count;

    /* sample rate in Hz, channels, and layout; default unset. */
    mc_audio_format format;
} mc_pcm_view;

#define MC_PCM_VIEW_DEFAULT { sizeof(mc_pcm_view), NULL, 0, MC_AUDIO_FORMAT_DEFAULT }

/* describes an input before opening it, so a factory can choose an
implementation without reading or consuming stream data. */
typedef struct mc_media_hint {
    /* readable bytes; initialize with MC_MEDIA_HINT_DEFAULT. */
    uint32_t struct_size;

    /* non-owning UTF-8 URI, valid during the probe/create call; default empty. */
    mc_string_view uri;

    /* lowercase MIME type without parameters. empty means unknown. valid during the call. */
    mc_string_view content_type;

    /* non-owning lowercase extension without a dot; empty means unknown.
    valid during the call. probes must not perform IO or consume stream bytes. */
    mc_string_view extension;
} mc_media_hint;

#define MC_MEDIA_HINT_DEFAULT { sizeof(mc_media_hint), { NULL, 0 }, { NULL, 0 }, { NULL, 0 } }

/* host functions for growable decoder scratch buffers. allocation and growth
run on producer workers, never hardware callbacks. core separately manages a
fixed playback pool and gives outputs read-only mc_pcm_view slices and tokens;
this table cannot acquire, resize, or recycle those slices. */
typedef struct mc_audio_scratch_buffer_api {
    /* readable bytes; initialize with MC_AUDIO_SCRATCH_BUFFER_API_DEFAULT. */
    uint32_t struct_size;

    /* host-owned state, valid for the instance lifetime; default null. */
    void* context;

    /* buffer calls are serialized on producer workers, outside hardware callbacks.
    create an owned empty buffer with frame capacity in the supplied format.
    the caller releases it with destroy; error is optional and host-owned. */
    mc_result(MC_CALL* create)(void* context, const mc_audio_format* format, size_t frame_capacity,
        mc_audio_scratch_buffer** out_buffer, mc_error** error);

    /* replace the format outside an active write, clearing populated frames.
    preserve capacity when channels are unchanged; otherwise clear capacity too.
    the input is copied during the call. */
    mc_result(MC_CALL* set_format)(void* context, mc_audio_scratch_buffer* buffer, const mc_audio_format* format);

    /* ensure at least frame_capacity frames of scratch space. sufficient
    capacity is a no-op; growth may allocate and move storage on this worker.
    does not wait for playback space or change populated frames. failure leaves
    the buffer unchanged. no active write or retained PCM view may remain. */
    mc_result(MC_CALL* reserve)(void* context, mc_audio_scratch_buffer* buffer, size_t frame_capacity);

    /* return stable writable storage until end_write. nested writes are rejected;
    no resize, reformat, destruction, recycling, or other access is allowed meanwhile.
    failure grants no access. the returned view does not own the storage. */
    mc_result(MC_CALL* begin_write)(void* context, mc_audio_scratch_buffer* buffer, mc_audio_buffer_view* out_view);

    /* publish written_frames and end access. pair every successful begin_write,
    even on failure; zero discards a partial write. the pointer then expires.
    an over-capacity count ends access, publishes zero frames, and reports invalid
    argument. this cannot protect against writing beyond the supplied capacity. */
    mc_result(MC_CALL* end_write)(void* context, mc_audio_scratch_buffer* buffer, size_t written_frames);

    /* release only a buffer the plugin created; decoder-supplied buffers remain
    core-owned. clear the caller's slot; null is accepted.
    no active write or other use may remain. default callback is null. */
    void(MC_CALL* destroy)(void* context, mc_audio_scratch_buffer** buffer);

    /* read the published frames once, without copying samples. the view stays
    valid until the next write, resize, format change or destruction. the next
    read is empty until end_write publishes another block; a decoder that makes
    no progress cannot accidentally replay its previous samples. */
    mc_result(MC_CALL* read_block)(void* context, mc_audio_scratch_buffer* buffer, mc_pcm_view* view);
} mc_audio_scratch_buffer_api;

#define MC_AUDIO_SCRATCH_BUFFER_API_DEFAULT \
    { sizeof(mc_audio_scratch_buffer_api), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
