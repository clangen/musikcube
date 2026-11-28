#ifndef MUSIKCORE_PLUGIN_VISUALIZER_H
#define MUSIKCORE_PLUGIN_VISUALIZER_H

#include <musikcore/plugin/audio.h>
#include <musikcore/plugin/services.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_VISUALIZER "org.musikcube.visualizer"

/* A factory may accept PCM, spectrum, or both. Spectrum bins are decibels
above an -80 dBFS floor, clamped to [0, 80]. */
typedef uint32_t mc_visualizer_inputs;
enum { MC_VISUALIZER_INPUT_PCM = 1, MC_VISUALIZER_INPUT_SPECTRUM = 2 };

typedef struct mc_plugin_visualizer_vtable {
    uint32_t struct_size;
    void (MC_CALL* destroy)(void* instance);
    mc_result (MC_CALL* show)(void* instance);
    mc_result (MC_CALL* hide)(void* instance);
    /* return a bounded, thread-safe visibility snapshot. musikcore may call
    this from playback workers concurrently with show/hide and user-initiated
    window closes. do not wait for rendering, dispatch to the UI thread, or
    call host playback controls. audio callbacks do not call this operation. */
    mc_result (MC_CALL* is_visible)(void* instance, bool* visible);
    /* Audio views are borrowed for the call only. Calls can originate from
    multiple playback workers and can overlap show/hide or a user closing the
    window. Providers must ignore frames while hidden, be thread-safe, and
    must not block playback workers on rendering or UI. */
    mc_result (MC_CALL* write_pcm)(void* instance, const mc_pcm_view* pcm);
    mc_result (MC_CALL* write_spectrum)(void* instance, const float* bins, size_t count, uint32_t sample_rate);
} mc_plugin_visualizer_vtable;

#define MC_PLUGIN_VISUALIZER_VTABLE_DEFAULT { sizeof(mc_plugin_visualizer_vtable), NULL, NULL, NULL, NULL, NULL, NULL }

typedef struct mc_visualizer_factory {
    uint32_t struct_size;
    void* context;
    const mc_plugin_visualizer_vtable* instance_api;
    /* Name and table storage remain valid through plugin shutdown. */
    mc_string_view name;
    mc_visualizer_inputs inputs;
    /* On failure leave out_instance null; services remain valid until destroy. */
    mc_result (MC_CALL* create)(void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);
} mc_visualizer_factory;

#define MC_VISUALIZER_FACTORY_DEFAULT { sizeof(mc_visualizer_factory), NULL, NULL, { NULL, 0 }, 0, NULL }

#ifdef __cplusplus
}
#endif
#endif
