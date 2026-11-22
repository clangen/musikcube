#ifndef MUSIKCORE_VISUALIZER_H
#define MUSIKCORE_VISUALIZER_H

#include <musikcore/abi.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (MC_CALL *mc_visualizer_list_fn)(void* user_data, const mc_string_view* names, size_t count);

/* Call visualizer operations on the runtime's owner thread. The list is
   ordered by name and valid only during the callback. An empty runtime
   returns an empty list. */
MC_API mc_result MC_CALL mc_visualizer_list(mc_visualizer_list_fn read, void* user_data);
/* Show the visualizer at the given list index. Unknown indices return MC_ERROR_NOT_FOUND. */
MC_API mc_result MC_CALL mc_visualizer_show(size_t index);
/* Hide the open visualizer. Returns MC_ERROR_NOT_FOUND when none is visible. */
MC_API mc_result MC_CALL mc_visualizer_hide(void);
/* Report whether the selected visualizer is visible. Call on the runtime owner thread. */
MC_API mc_result MC_CALL mc_visualizer_is_visible(bool* out_visible);

enum { MC_VISUALIZER_SPECTRUM_BINS = 256 };

/* Enable capture for the managed runtime. Disabled by default. */
MC_API mc_result MC_CALL mc_visualizer_set_spectrum_enabled(bool enabled);
/* Copy one FFT frame into a caller-owned array of at least MC_VISUALIZER_SPECTRUM_BINS floats.
   Bins are separated by sample_rate_hz / 512 Hz, starting at DC. Values are dB above an
   -80 dBFS floor, clamped to [0, 80]. The Nyquist bin is omitted.
   Returns MC_ERROR_NOT_FOUND until a frame is available, and MC_ERROR_BUSY
   if the frame changes during the copy. Outputs are unchanged on failure. */
MC_API mc_result MC_CALL mc_visualizer_read_spectrum(float* bins, size_t capacity, uint32_t* sample_rate_hz);

#ifdef __cplusplus
}
#endif

#endif
