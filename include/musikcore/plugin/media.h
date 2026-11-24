#ifndef MUSIKCORE_PLUGIN_MEDIA_H
#define MUSIKCORE_PLUGIN_MEDIA_H

#include <musikcore/plugin/decoder.h>
#include <musikcore/plugin/encoder.h>

#ifdef __cplusplus
extern "C" {
#endif

/* lets a plugin open another resource through the same providers used by
playback. a subsong source uses this for its container; the server uses it
to select the decoder for a requested track. calls may run on workers. */
typedef struct mc_media_host_api {
    /* readable bytes; initialize with MC_MEDIA_HOST_API_DEFAULT. */
    uint32_t struct_size;

    /* host-owned session state. pass unchanged to each operation. */
    void* context;

    /* select by URI without IO. on success the caller owns an unopened
    stream and can publish it for cancellation before calling open. */
    mc_result (MC_CALL* create_stream)(void* context, mc_string_view uri, mc_stream* out_stream, mc_error** error);

    /* select using the opened stream's metadata without consuming bytes.
    the caller owns the unopened decoder and keeps the stream alive until
    decoder destruction. unsupported formats return MC_ERROR_UNSUPPORTED. */
    mc_result (MC_CALL* create_decoder)(
        void* context, const mc_stream* opened_stream, mc_decoder* out_decoder, mc_error** error);

    /* select a compressor by extension or MIME type. the caller destroys it. */
    mc_result (MC_CALL* create_encoder)(
        void* context, mc_string_view format, mc_encoder* out_encoder, mc_error** error);
} mc_media_host_api;

#define MC_MEDIA_HOST_API_DEFAULT { sizeof(mc_media_host_api), NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
