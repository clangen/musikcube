#ifndef MUSIKCORE_PLUGINS_H
#define MUSIKCORE_PLUGINS_H

#include <musikcore/preferences.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_plugin_info {
    /* Identifier declared by the plugin. */
    mc_string_view id;
    /* Human-readable display name. */
    mc_string_view name;
    /* Version declared by the plugin. */
    mc_string_view version;
    /* Binary filename within the plugin directory. */
    mc_string_view filename;
    /* Whether the plugin is enabled in the saved configuration. */
    bool enabled;
} mc_plugin_info;

typedef struct mc_plugin_setting {
    /* Preference key declared by the plugin. */
    mc_string_view key;
    /* Configured value, falling back to the plugin's default. */
    mc_preference_value value;
    /* Inclusive lower and upper limits for numeric values. */
    double minimum;
    double maximum;
    /* Decimal places to display for a numeric value. */
    int32_t precision;
    /* Allowed text choices. Null for unrestricted values. */
    const mc_string_view* choices;
    /* Number of choices. */
    size_t choice_count;
    /* Whether an unrestricted text setting accepts an empty edit. */
    bool allow_empty;
} mc_plugin_setting;

typedef void (MC_CALL *mc_plugin_info_fn)(void* user_data, const mc_plugin_info* plugin);
typedef void (MC_CALL *mc_plugin_setting_fn)(void* user_data, const mc_plugin_setting* setting);

/* Call on the runtime owner thread. List installed plugins, including disabled
   plugins. Rows and string data are borrowed for each callback. */
MC_API mc_result MC_CALL mc_plugins_list(mc_plugin_info_fn read, void* user_data);
/* Persist the enabled preference for the installed filename. Already-created
   plugin instances remain valid. */
MC_API mc_result MC_CALL mc_plugin_set_enabled(mc_string_view filename, bool enabled);
/* Read the plugin's exposed settings by ID. A missing schema returns
   MC_ERROR_UNSUPPORTED. Rows are borrowed for each callback. */
MC_API mc_result MC_CALL mc_plugin_read_settings(mc_string_view id, mc_plugin_setting_fn read, void* user_data);
/* Validate and persist a setting declared by the plugin's schema. */
MC_API mc_result MC_CALL mc_plugin_write_setting(mc_string_view id, const mc_preference_entry* entry);

#ifdef __cplusplus
}
#endif

#endif
