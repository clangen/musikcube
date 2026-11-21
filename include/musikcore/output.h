#ifndef MUSIKCORE_OUTPUT_H
#define MUSIKCORE_OUTPUT_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_output_device {
    /* Stable device identifier scoped to its driver. */
    mc_string_view id;
    /* Display name for the device. */
    mc_string_view name;
} mc_output_device;

typedef struct mc_output_configuration {
    /* Available driver names. */
    const mc_string_view* drivers;
    /* Number of available drivers. */
    size_t driver_count;
    /* Effective driver name after applying fallback. */
    mc_string_view selected_driver;
    /* Available devices for the effective driver. */
    const mc_output_device* devices;
    /* Number of available devices. */
    size_t device_count;
    /* Current default device. Empty fields indicate the system default. */
    mc_output_device selected_device;
} mc_output_configuration;

typedef void (MC_CALL *mc_output_configuration_fn)(void* user_data, const mc_output_configuration* configuration);

/* Call on the runtime's owner thread. The snapshot is valid only during read.
   The ABI-only runtime returns MC_ERROR_UNSUPPORTED. */
MC_API mc_result MC_CALL mc_output_read_configuration(mc_output_configuration_fn read, void* user_data);
/* Select an available driver by name. Unknown names return MC_ERROR_NOT_FOUND. */
MC_API mc_result MC_CALL mc_output_select_driver(mc_string_view driver);
/* Select a device on the effective driver. An empty ID restores its default.
   Returns MC_ERROR_NOT_FOUND if the driver changed or the device is unavailable. */
MC_API mc_result MC_CALL mc_output_select_device(mc_string_view driver, mc_string_view device_id);

#ifdef __cplusplus
}
#endif

#endif
