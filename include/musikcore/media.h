#ifndef MUSIKCORE_MEDIA_H
#define MUSIKCORE_MEDIA_H

#include <musikcore/context.h>

#ifdef __cplusplus
extern "C" {
#endif

/* application-owned media resources, independent of playback and the plugin
SDK. create streams on the context owner thread with MC_SERVICE_PLUGINS.
ordinary operations may block and belong on workers, never audio callbacks.
concurrent ordinary calls on one resource return BUSY; interrupt may race them.
interrupt latches cancellation; further ordinary operations return CANCELLED.
encoder interruption wakes output IO; provider CPU work must return before
cleanup can complete. destroy and finish still release cancelled resources.
synchronize handle destruction/ownership transfer with all uses of that handle.
context shutdown interrupts and drains calls before unloading providers. retained
handles then return NOT_FOUND and remain destroyable; PCM stays readable. */
typedef struct mc_media_stream mc_media_stream;
typedef struct mc_media_decoder mc_media_decoder;
typedef struct mc_media_encoder mc_media_encoder;
typedef struct mc_media_pcm mc_media_pcm;

typedef uint32_t mc_media_open_flags;
enum {
    MC_MEDIA_OPEN_READ = 1u,
    MC_MEDIA_OPEN_WRITE = 2u,
    MC_MEDIA_OPEN_CREATE = 4u,
    MC_MEDIA_OPEN_TRUNCATE = 8u,
    MC_MEDIA_OPEN_APPEND = 16u
};
enum {
    MC_MEDIA_READABLE = 1u,
    MC_MEDIA_WRITABLE = 2u,
    MC_MEDIA_SEEKABLE = 4u,
    MC_MEDIA_LENGTH_KNOWN = 8u,
    MC_MEDIA_CAN_PREFETCH = 16u
};
typedef uint32_t mc_media_read_state;
enum { MC_MEDIA_MORE = 0, MC_MEDIA_END = 1 };

/* describes an opened stream without exposing provider-owned storage. */
typedef struct mc_media_stream_info {
    /* available bytes; initialize with MC_MEDIA_STREAM_INFO_DEFAULT. */
    uint32_t struct_size;
    /* MC_MEDIA_READABLE and related capability bits; default zero. */
    uint32_t flags;
    /* total bytes when MC_MEDIA_LENGTH_KNOWN is set; default zero. */
    uint64_t length_bytes;
    /* borrowed UTF-8 URI, possibly containing credentials, until handle destruction. */
    mc_string_view uri;
    /* borrowed MIME type until handle destruction; empty means unknown. */
    mc_string_view content_type;
    /* borrowed extension without a dot until handle destruction; empty means unknown. */
    mc_string_view extension;
} mc_media_stream_info;
#define MC_MEDIA_STREAM_INFO_DEFAULT { sizeof(mc_media_stream_info), 0, 0, { NULL, 0 }, { NULL, 0 }, { NULL, 0 } }

/* read-only interleaved float32 audio; a view does not own its samples. */
typedef struct mc_media_pcm_view {
    /* available bytes; initialize with MC_MEDIA_PCM_VIEW_DEFAULT. */
    uint32_t struct_size;
    /* frame_count * format.channels samples, borrowed for the operation. */
    const float* data;
    /* complete interleaved frames, not individual samples; default zero. */
    size_t frame_count;
    /* sample rate in Hz, channel count and layout; default unset. */
    mc_audio_format format;
} mc_media_pcm_view;
#define MC_MEDIA_PCM_VIEW_DEFAULT { sizeof(mc_media_pcm_view), NULL, 0, MC_AUDIO_FORMAT_DEFAULT }

enum { MC_MEDIA_DEFAULT_BITRATE_KBPS = 128u };

/* fixes an encoder's input format and compression rate before opening it. */
typedef struct mc_media_encoder_options {
    /* available bytes; initialize with MC_MEDIA_ENCODER_OPTIONS_DEFAULT. */
    uint32_t struct_size;
    /* required positive sample rate in Hz and channel count; default unset. */
    mc_audio_format format;
    /* positive target kilobits per second; default 128. lossless codecs may ignore it. */
    uint32_t bitrate_kbps;
} mc_media_encoder_options;
#define MC_MEDIA_ENCODER_OPTIONS_DEFAULT \
    { sizeof(mc_media_encoder_options), MC_AUDIO_FORMAT_DEFAULT, MC_MEDIA_DEFAULT_BITRATE_KBPS }

/* selects a provider without IO and copies uri. accepts a nonempty UTF-8 path
or provider URI, with no embedded NUL. create/truncate/append require write;
append and truncate are mutually exclusive. output slots must be empty.
optional error slots must be empty and receive owned mc_error diagnostics. */
MC_API mc_result MC_CALL mc_media_stream_create(
    mc_context* context, mc_string_view uri, mc_media_open_flags flags, mc_media_stream** out_stream, mc_error** error);

/* opens once, including after failure. publish the handle before blocking so
another thread can interrupt it. interruption is latched, including before open. */
MC_API mc_result MC_CALL mc_media_stream_open(mc_media_stream* stream, mc_error** error);

MC_API mc_result MC_CALL mc_media_stream_interrupt(mc_media_stream* stream);

MC_API mc_result MC_CALL mc_media_stream_get_info(const mc_media_stream* stream, mc_media_stream_info* info);

/* EOF may accompany the final bytes. positive reads make progress, reach EOF,
or fail. transferred bytes are reported even on IO failure; zero capacity is valid. */
MC_API mc_result MC_CALL mc_media_stream_read(mc_media_stream* stream, void* data, size_t capacity_bytes,
    size_t* read_bytes, mc_media_read_state* state, mc_error** error);

/* partial writes are reported even on failure. callers must advance by written_bytes. */
MC_API mc_result MC_CALL mc_media_stream_write(
    mc_media_stream* stream, const void* data, size_t size_bytes, size_t* written_bytes, mc_error** error);

MC_API mc_result MC_CALL mc_media_stream_seek(mc_media_stream* stream, uint64_t byte_offset, mc_error** error);

MC_API mc_result MC_CALL mc_media_stream_tell(const mc_media_stream* stream, uint64_t* byte_offset);

/* closes and destroys, reporting close/flush failure while still clearing the
slot. BUSY preserves the slot. NULL slots are accepted by every destroy call. */
MC_API mc_result MC_CALL mc_media_stream_destroy(mc_media_stream** stream);

/* selects an unopened codec. success consumes and clears the opened readable
input; failure preserves it. decoder destruction releases its input last. */
MC_API mc_result MC_CALL mc_media_decoder_create(
    mc_media_stream** input, mc_media_decoder** out_decoder, mc_error** error);

MC_API mc_result MC_CALL mc_media_decoder_open(mc_media_decoder* decoder, mc_error** error);

MC_API mc_result MC_CALL mc_media_decoder_interrupt(mc_media_decoder* decoder);

/* returns owned PCM or NULL for an empty final block. END can accompany PCM.
PCM remains valid across further reads and context shutdown. */
MC_API mc_result MC_CALL mc_media_decoder_read(
    mc_media_decoder* decoder, mc_media_pcm** out_pcm, mc_media_read_state* state, mc_error** error);

/* finite nonnegative seconds; actual_seconds reports codec seek granularity. */
MC_API mc_result MC_CALL mc_media_decoder_seek(
    mc_media_decoder* decoder, double seconds, double* actual_seconds, mc_error** error);

/* duration in seconds; UNSUPPORTED when unknown. */
MC_API mc_result MC_CALL mc_media_decoder_get_duration(const mc_media_decoder* decoder, double* seconds);

MC_API mc_result MC_CALL mc_media_decoder_destroy(mc_media_decoder** decoder);

/* view samples remain valid until this owned PCM handle is destroyed. */
MC_API mc_result MC_CALL mc_media_pcm_get_view(const mc_media_pcm* pcm, mc_media_pcm_view* view);

MC_API void MC_CALL mc_media_pcm_destroy(mc_media_pcm** pcm);

/* selects by extension or MIME type, copying options. success consumes an
opened writable output; failure preserves it. works with streaming and file
encoders; core writes streaming blocks to the output, including partial writes. */
MC_API mc_result MC_CALL mc_media_encoder_create(mc_media_stream** output, mc_string_view format,
    const mc_media_encoder_options* options, mc_media_encoder** out_encoder, mc_error** error);

MC_API mc_result MC_CALL mc_media_encoder_open(mc_media_encoder* encoder, mc_error** error);

MC_API mc_result MC_CALL mc_media_encoder_interrupt(mc_media_encoder* encoder);

/* copies/consumes PCM during the call. its format must match options exactly.
a failed write poisons the encoder; finish releases it without retrying bytes. */
MC_API mc_result MC_CALL mc_media_encoder_write(
    mc_media_encoder* encoder, const mc_media_pcm_view* pcm, mc_error** error);

/* flush once, finalize, destroy the codec, then close output. reports the first
failure and releases resources even on failure. repeat calls return that result.
destroy without finish aborts output without flushing/finalizing the codec. */
MC_API mc_result MC_CALL mc_media_encoder_finish(mc_media_encoder* encoder, mc_error** error);

MC_API mc_result MC_CALL mc_media_encoder_destroy(mc_media_encoder** encoder);

#ifdef __cplusplus
}
#endif
#endif
