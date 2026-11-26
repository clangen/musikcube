#pragma once

#include <musikcore/plugin/audio.h>

namespace musik { namespace core { namespace ffi { namespace audio_scratch_buffers {

    /* growable decoder scratch storage, separate from the fixed playback pool.
    operations are caller-serialized worker calls. core copies/processes scratch
    PCM into available playback slices before queuing them to an output.
    Read exposes samples only outside a write; its view expires on the next
    mutation or destruction. neither operation manages playback slices. */
    const mc_plugin_audio_scratch_service& Host();
    mc_result Read(const mc_audio_scratch_buffer* buffer, mc_pcm_view* view);

} } } }
