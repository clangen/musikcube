#ifndef MUSIKCORE_PLUGIN_SETTINGS_H
#define MUSIKCORE_PLUGIN_SETTINGS_H

#include <musikcore/setting_types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_CONFIGURATION "org.musikcube.configuration"

/* describes one adjustable integer. the plugin and settings UI share this
definition so they agree on what a saved value means. */
typedef struct mc_integer_setting {
    /* case-sensitive UTF-8 key in the plugin's setting file. */
    mc_string_view key;

    /* value used when the setting is absent or outside its declared range. */
    int32_t default_value;

    /* smallest allowed value, in this setting's own units. */
    int32_t minimum;
    /* largest allowed value. */
    int32_t maximum;
} mc_integer_setting;

/* describes a plugin option that can be switched on or off. */
typedef struct mc_boolean_setting {
    /* case-sensitive UTF-8 setting key. */
    mc_string_view key;

    /* value used when no valid setting is saved. */
    bool default_value;
} mc_boolean_setting;

/* describes an adjustable floating-point plugin option. */
typedef struct mc_number_setting {
    /* case-sensitive UTF-8 setting key. */
    mc_string_view key;

    /* finite value used when no valid setting is saved. */
    double default_value;

    /* smallest permitted value, inclusive. */
    double minimum;

    /* largest permitted value, inclusive. */
    double maximum;

    /* decimal places displayed by the settings UI. */
    int32_t precision;
} mc_number_setting;

/* describes an unrestricted UTF-8 plugin setting. */
typedef struct mc_text_setting {
    /* case-sensitive UTF-8 setting key. */
    mc_string_view key;

    /* fallback used when no value is saved. */
    mc_string_view default_value;
} mc_text_setting;

/* register this immutable description to expose the plugin's settings in the
application. keys and arrays must remain valid until plugin shutdown. */
typedef struct mc_configuration_api {
    /* available bytes. initialize with MC_CONFIGURATION_API_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned integer descriptions, or null when none are declared. */
    const mc_integer_setting* integers;

    /* number of elements in integers. zero by default. */
    size_t integer_count;

    /* plugin-owned boolean descriptions, or null. */
    const mc_boolean_setting* booleans;

    /* number of boolean descriptions. */
    size_t boolean_count;

    /* plugin-owned floating-point descriptions, or null. */
    const mc_number_setting* numbers;

    /* number of floating-point descriptions. */
    size_t number_count;

    /* plugin-owned text descriptions, or null. */
    const mc_text_setting* texts;

    /* number of text descriptions. */
    size_t text_count;
} mc_configuration_api;

#define MC_CONFIGURATION_API_DEFAULT { sizeof(mc_configuration_api), NULL, 0, NULL, 0, NULL, 0, NULL, 0 }

/* receives a setting value during a synchronous read. copy it to retain it. */
typedef void (MC_CALL* mc_setting_text_fn)(void* user_data, mc_string_view value);

/* access the setting store scoped to the factory's plugin. callbacks are
thread-safe and valid through instance destruction; keep them off real-time
callbacks because writes perform disk I/O. a stream can snapshot its settings
at creation so later edits only affect new requests. */
typedef struct mc_plugin_settings_service {
    /* available bytes. initialize with MC_PLUGIN_SETTINGS_SERVICE_DEFAULT. */
    uint32_t struct_size;

    /* host-owned settings store. pass unchanged to every callback. */
    void* context;

    /* read a setting with its declared fallback and bounds. the descriptor
    is needed only during the call. returns MC_OK for an absent setting. */
    mc_result(MC_CALL* read_integer)(void* context, const mc_integer_setting* setting, int32_t* value);

    /* read a saved boolean, using fallback if it is absent or invalid. */
    mc_result(MC_CALL* read_boolean)(void* context, mc_string_view key, bool fallback, bool* value);

    /* read a finite number, using the finite fallback if it is absent or invalid. */
    mc_result(MC_CALL* read_number)(void* context, mc_string_view key, double fallback, double* value);

    /* read text with a fallback; the callback's view is valid only during the call. */
    mc_result(MC_CALL* read_text)(void* context, mc_string_view key, mc_string_view fallback,
        mc_setting_text_fn read, void* user_data);

    /* validate a batch and persist it before publishing the in-memory update.
    inputs are borrowed only for this call. zero entries are valid; later
    duplicate keys win. invalid entries return MC_ERROR_INVALID_ARGUMENT;
    persistence failures return MC_ERROR_IO. both leave in-memory values
    unchanged; physical file replacement is not guaranteed atomic on I/O
    failure. does not reconfigure instances; the caller owns that sequencing. */
    mc_result(MC_CALL* write_values)(void* context, const mc_setting_entry* entries, size_t count);
} mc_plugin_settings_service;

#define MC_PLUGIN_SETTINGS_SERVICE_DEFAULT { sizeof(mc_plugin_settings_service), NULL, NULL, NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
