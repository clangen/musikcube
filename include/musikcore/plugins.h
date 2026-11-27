#ifndef MUSIKCORE_PLUGINS_H
#define MUSIKCORE_PLUGINS_H

#include <musikcore/settings.h>

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
    /* Setting key declared by the plugin. */
    mc_string_view key;
    /* Configured value, falling back to the plugin's default. */
    mc_setting_value value;
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

/* list installed plugins, including disabled plugins. call on the runtime
owner thread after starting MC_SERVICE_PLUGINS. returns MC_ERROR_WRONG_THREAD
from another thread and MC_ERROR_UNSUPPORTED when the
plugin service is not running. rows and strings are valid only during each
callback; copy them to retain a snapshot. */
MC_API mc_result MC_CALL mc_plugins_list(mc_plugin_info_fn read, void* user_data);
/* The operations below require the runtime owner thread and MC_SERVICE_PLUGINS.
Persist the enabled setting for the installed filename. Already-created
   plugin instances remain valid. */
MC_API mc_result MC_CALL mc_plugin_set_enabled(mc_string_view filename, bool enabled);
/* Read the plugin's exposed settings by ID. A missing schema returns
   MC_ERROR_UNSUPPORTED. Rows are borrowed for each callback. */
MC_API mc_result MC_CALL mc_plugin_read_settings(mc_string_view id, mc_plugin_setting_fn read, void* user_data);
/* validate every entry against the plugin schema before persisting the batch.
if any entry is invalid or a key is repeated, none are saved. */
MC_API mc_result MC_CALL mc_plugin_write_settings(mc_string_view id, const mc_setting_entry* entries, size_t count);

/* validate and persist a setting declared by the plugin's schema. */
MC_API mc_result MC_CALL mc_plugin_write_setting(mc_string_view id, const mc_setting_entry* entry);

/* restart a running playback-remote integration to apply saved settings.
call on the runtime owner thread. returns MC_ERROR_UNSUPPORTED when the plugin
does not provide a running playback remote. saved settings remain if restart
fails; the integration stays stopped. */
MC_API mc_result MC_CALL mc_plugin_apply_settings(mc_string_view id);

#ifdef __cplusplus
}
#endif

#endif
