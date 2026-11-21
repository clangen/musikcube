#ifndef MUSIKCORE_EQUALIZER_H
#define MUSIKCORE_EQUALIZER_H

#include <musikcore/context.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { MC_EQUALIZER_BAND_COUNT = 18 };

typedef struct mc_equalizer_band {
    /* Center frequency in Hz. */
    uint32_t frequency;
    /* Gain in decibels. */
    double gain;
} mc_equalizer_band;

typedef struct mc_equalizer_state {
    /* Size in bytes. Initialize with MC_EQUALIZER_STATE_DEFAULT. */
    uint32_t struct_size;
    /* Whether equalization is enabled. */
    bool enabled;
    /* Bands ordered by ascending center frequency. */
    mc_equalizer_band bands[MC_EQUALIZER_BAND_COUNT];
} mc_equalizer_state;

#define MC_EQUALIZER_STATE_DEFAULT { sizeof(mc_equalizer_state), false, { { 0, 0.0 } } }

/* Use on the runtime owner thread. Returns MC_ERROR_UNSUPPORTED without an
   equalizer. On failure, state is unchanged. */
MC_API mc_result MC_CALL mc_equalizer_get_state(mc_equalizer_state* state);
MC_API mc_result MC_CALL mc_equalizer_set_enabled(bool enabled);
/* Supply exactly MC_EQUALIZER_BAND_COUNT finite gains between -20 and +20 dB. */
MC_API mc_result MC_CALL mc_equalizer_set_gains(const double* gains, size_t count);

typedef void (MC_CALL *mc_equalizer_changed_fn)(void* user_data);

/* Register on the dispatcher thread. Dispatch coalesces updates. Pass NULL
   to remove the observer and discard any queued update. */
MC_API mc_result MC_CALL mc_context_set_equalizer_changed_callback(
    mc_context* context, mc_equalizer_changed_fn callback, void* user_data);

#ifdef __cplusplus
}
#endif
#endif
