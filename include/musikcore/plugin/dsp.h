#ifndef MUSIKCORE_PLUGIN_DSP_H
#define MUSIKCORE_PLUGIN_DSP_H

#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_DSP "org.musikcube.dsp"

/* processes one playback stream's audio in place on its decoding worker. */
typedef struct mc_dsp_vtable {
    /* available bytes. initialize with MC_DSP_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* release the instance after its worker has stopped. */
    void (MC_CALL* destroy)(void* instance);

    /* modify interleaved frames in place. the view expires on return. calls
    for one instance are serialized and never run on a hardware callback.
    on failure the host discards this block and stops playback. */
    mc_result (MC_CALL* process)(void* instance, mc_audio_buffer_view* audio);
} mc_dsp_vtable;

#define MC_DSP_VTABLE_DEFAULT { sizeof(mc_dsp_vtable), NULL, NULL }

/* creates independent processors for playback streams and notifies them when
saved settings change. reload may run concurrently with instance operations. */
typedef struct mc_dsp_factory {
    /* available bytes. initialize with MC_DSP_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned factory state, or null. */
    void* context;

    /* immutable operations, valid through instance destruction. */
    const mc_dsp_vtable* instance_api;

    /* create an instance in an empty slot. failure leaves the slot empty.
    services and their referenced setting store remain valid until destroy. */
    mc_result (MC_CALL* create)(
        void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);

    /* notify existing instances that their saved settings have changed. */
    void (MC_CALL* reload)(void* context);
} mc_dsp_factory;

#define MC_DSP_FACTORY_DEFAULT { sizeof(mc_dsp_factory), NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
