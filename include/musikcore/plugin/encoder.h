#ifndef MUSIKCORE_PLUGIN_ENCODER_H
#define MUSIKCORE_PLUGIN_ENCODER_H

#include <musikcore/plugin/stream.h>
#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_ENCODER "org.musikcube.encoder"

/* one compressed block owned by the encoder until its next operation. */
typedef struct mc_encoded_block {
    /* available bytes. initialize with MC_ENCODED_BLOCK_DEFAULT. */
    uint32_t struct_size;
    /* bytes produced, or null when more input is needed. */
    const char* data;
    /* length in bytes. zero when no bytes were produced. */
    size_t length;
} mc_encoded_block;

#define MC_ENCODED_BLOCK_DEFAULT { sizeof(mc_encoded_block), NULL, 0 }

/* compresses PCM either into returned blocks or a host-owned writable stream. */
typedef struct mc_encoder_vtable {
    /* available bytes. initialize with MC_ENCODER_VTABLE_DEFAULT. */
    uint32_t struct_size;
    /* release the encoder before its output stream. */
    void (MC_CALL* destroy)(void* instance);
    /* one for a streaming encoder, zero for an encoder requiring a stream. */
    uint32_t streaming;
    /* configure interleaved PCM and bitrate in kilobits per second.
    output is null for streaming encoders and stays open through finalize otherwise. */
    mc_result (MC_CALL* initialize)(void* instance, const mc_stream* output,
        uint32_t sample_rate_hz, uint32_t channels, uint32_t bitrate_kbps);
    /* consume one PCM block and return any encoded bytes for streaming. */
    mc_result (MC_CALL* encode)(void* instance, const mc_pcm_view* pcm, mc_encoded_block* block);
    /* emit delayed bytes after the last PCM block. */
    mc_result (MC_CALL* flush)(void* instance, mc_encoded_block* block);
    /* complete the stream, or update metadata in the completed file at uri. */
    mc_result (MC_CALL* finalize)(void* instance, mc_string_view uri);
} mc_encoder_vtable;

#define MC_ENCODER_VTABLE_DEFAULT { sizeof(mc_encoder_vtable), NULL, 0, NULL, NULL, NULL, NULL }

/* owns one encoder instance until destroy. */
typedef struct mc_encoder {
    /* available bytes. initialize with MC_ENCODER_DEFAULT. */
    uint32_t struct_size;
    /* plugin-owned encoder state. null by default. */
    void* instance;
    /* plugin-owned operations, valid through destroy. null by default. */
    const mc_encoder_vtable* api;
} mc_encoder;

#define MC_ENCODER_DEFAULT { sizeof(mc_encoder), NULL, NULL }

/* selects an encoder by extension or MIME type. factory calls may run concurrently. */
typedef struct mc_encoder_factory {
    /* available bytes. initialize with MC_ENCODER_FACTORY_DEFAULT. */
    uint32_t struct_size;
    /* plugin-owned shared state, or null. */
    void* context;
    /* inspect a call-scoped format without creating an instance. */
    mc_result (MC_CALL* can_handle)(void* context, mc_string_view format, uint32_t* supported);
    /* create an encoder in an empty slot. failure leaves it empty. */
    mc_result (MC_CALL* create)(void* context, const mc_plugin_services* services,
        mc_string_view format, mc_encoder* output, mc_error** error);
} mc_encoder_factory;

#define MC_ENCODER_FACTORY_DEFAULT { sizeof(mc_encoder_factory), NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
