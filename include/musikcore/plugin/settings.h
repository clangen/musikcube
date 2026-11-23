#ifndef MUSIKCORE_PLUGIN_SETTINGS_H
#define MUSIKCORE_PLUGIN_SETTINGS_H

#include <musikcore/abi.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_CONFIGURATION "org.musikcube.configuration"

/* describes one adjustable integer. the plugin and settings UI share this
definition so they agree on what a saved value means. */
typedef struct mc_integer_setting {
    /* case-sensitive UTF-8 key in the plugin's preference file. */
    mc_string_view key;

    /* value used when the preference is absent or outside its declared range. */
    int32_t default_value;

    /* smallest allowed value, in this setting's own units. */
    int32_t minimum;
    /* largest allowed value. */
    int32_t maximum;
} mc_integer_setting;

/* register this immutable description to expose the plugin's settings in the
application. keys and arrays must remain valid until plugin shutdown. */
typedef struct mc_configuration_api {
    /* available bytes. initialize with MC_CONFIGURATION_API_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned integer descriptions, or null when none are declared. */
    const mc_integer_setting* integers;

    /* number of elements in integers. zero by default. */
    size_t integer_count;
} mc_configuration_api;

#define MC_CONFIGURATION_API_DEFAULT { sizeof(mc_configuration_api), NULL, 0 }

/* read the saved preferences scoped to the factory's plugin. a stream can
snapshot settings at creation so later edits only affect new requests. */
typedef struct mc_preferences_host_api {
    /* available bytes. initialize with MC_PREFERENCES_HOST_API_DEFAULT. */
    uint32_t struct_size;

    /* host-owned preference store. pass unchanged to read_integer. */
    void* context;

    /* read a setting with its declared fallback and bounds. the descriptor
    is needed only during the call. returns MC_OK for an absent preference. */
    mc_result(MC_CALL* read_integer)(void* context, const mc_integer_setting* setting, int32_t* value);

    /* read a saved boolean, using fallback if it is absent or invalid. */
    mc_result(MC_CALL* read_boolean)(void* context, mc_string_view key, bool fallback, bool* value);

    /* read a finite number, using the finite fallback if it is absent or invalid. */
    mc_result(MC_CALL* read_number)(void* context, mc_string_view key, double fallback, double* value);
} mc_preferences_host_api;

#define MC_PREFERENCES_HOST_API_DEFAULT { sizeof(mc_preferences_host_api), NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
