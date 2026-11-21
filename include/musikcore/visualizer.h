#ifndef MUSIKCORE_VISUALIZER_H
#define MUSIKCORE_VISUALIZER_H

#include <musikcore/abi.h>

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

#ifdef __cplusplus
}
#endif

#endif
