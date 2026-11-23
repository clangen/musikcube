#ifndef MUSIKCORE_PLUGIN_DECODER_H
#define MUSIKCORE_PLUGIN_DECODER_H

#include <musikcore/plugin/stream.h>
#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_DECODER "org.musikcube.decoder"

typedef uint32_t mc_decode_state;
enum { MC_DECODE_MORE = 0, MC_DECODE_END = 1 };

/* turns one opened byte stream into interleaved floating-point PCM. ordinary
calls are serialized on the decoding worker. errors may return a host-owned diagnostic. */
typedef struct mc_decoder_vtable {
    /* available bytes. initialize with MC_DECODER_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* release the decoder before the host releases its input stream. */
    void (MC_CALL* destroy)(void* instance);

    /* attach an already-open stream. the host keeps the handle and its instance alive. */
    mc_result (MC_CALL* open)(void* instance, const mc_stream* stream, mc_error** error);

    /* latch cancellation. may run concurrently, including before open.
    the host also interrupts the input stream to wake blocked IO. */
    void (MC_CALL* interrupt)(void* instance);

    /* publish frames into host-owned scratch, ending the write before return.
    MC_DECODE_END may accompany the last frames. MC_DECODE_MORE requires progress. */
    mc_result (MC_CALL* decode)(void* instance, mc_audio_scratch_buffer* buffer, mc_decode_state* state,
        mc_error** error);

    /* seek to a finite, nonnegative time in seconds. actual_seconds is the
    decoder's playback position after success. packet-based demuxers that only
    confirm acceptance report the requested time; the first frame can differ
    by the codec's seek granularity. */
    mc_result (MC_CALL* seek)(void* instance, double requested_seconds, double* actual_seconds, mc_error** error);

    /* report duration in seconds, or MC_ERROR_UNSUPPORTED if unknown. */
    mc_result (MC_CALL* get_duration)(void* instance, double* seconds);

    /* suggest a positive output rate before decoding. unsupported leaves the native rate in effect. */
    mc_result (MC_CALL* set_preferred_sample_rate)(void* instance, uint32_t sample_rate_hz);
} mc_decoder_vtable;

#define MC_DECODER_VTABLE_DEFAULT { sizeof(mc_decoder_vtable), NULL, NULL, NULL, NULL, NULL, NULL, NULL }

/* one codec instance. the creator owns destroy and keeps the input stream
alive until it returns. copying this handle does not create another owner. */
typedef struct mc_decoder {
    /* available bytes. initialize with MC_DECODER_DEFAULT. */
    uint32_t struct_size;

    /* opaque codec state. a default handle has no instance. */
    void* instance;

    /* plugin-owned operations, valid through instance destruction. default null. */
    const mc_decoder_vtable* api;
} mc_decoder;

#define MC_DECODER_DEFAULT { sizeof(mc_decoder), NULL, NULL }

/* selects a codec by media hints. separate instances have independent decode
positions and may run concurrently for playback and server transcoding. */
typedef struct mc_decoder_factory {
    /* available bytes. initialize with MC_DECODER_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned state shared by concurrent factory calls. may be null. */
    void* context;

    /* immutable operations shared by instances until plugin shutdown. required. */
    const mc_decoder_vtable* instance_api;

    /* inspect call-scoped hints without consuming any input. write zero or one to supported. */
    mc_result (MC_CALL* can_handle)(void* context, const mc_media_hint* hint, uint32_t* supported);

    /* create an unopened decoder in an empty slot. on failure leave the slot empty. */
    mc_result (MC_CALL* create)(
        void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);
} mc_decoder_factory;

#define MC_DECODER_FACTORY_DEFAULT { sizeof(mc_decoder_factory), NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
