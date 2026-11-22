#include <musikcore/output.h>
#include <musikcore/equalizer.h>
#include <musikcore/playback.h>
#include <musikcore/visualizer.h>
#include <musikcore/plugins.h>
#include <musikcore/paths.h>
#include <musikcore/locale.h>
#include <musikcore/online.h>
#include "../common/error.h"

#include <cmath>

#include "../common/primitives.h"
#include "request.h"
#include "preferences-shape.h"
#include "server-shape.h"

std::unique_ptr<musik::core::ffi::ContextRuntime> musik::core::ffi::make_context_runtime() {
    return {};
}

mc_result MC_CALL mc_preferences_read(mc_preference_scope scope, mc_string_view key,
    const mc_preference_value* fallback, mc_preference_read_fn read, void*)
{
    using namespace musik::core::ffi;
    return valid_preference_scope(scope) && valid_text(key, false) && read &&
        fallback && valid_preference_value(*fallback)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_preferences_write(mc_preference_scope scope, const mc_preference_entry* entries, size_t count) {
    using namespace musik::core::ffi;
    if (!valid_preference_scope(scope) || !valid_preference_entries(entries, count)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return count ? MC_ERROR_UNSUPPORTED : MC_OK;
}

mc_result MC_CALL mc_preferences_save(mc_preference_scope scope) {
    return musik::core::ffi::valid_preference_scope(scope) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_preferences_list_keys(mc_preference_scope scope, mc_preference_key_fn read, void*) {
    return read && musik::core::ffi::valid_preference_scope(scope) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_server_read_configuration(mc_server_configuration_fn read, void*) {
    return read ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_server_configure(const mc_server_configuration* configuration) {
    return musik::core::ffi::valid_server_configuration(configuration) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_plugins_list(mc_plugin_info_fn read, void*) {
    return read ? MC_OK : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_plugin_set_enabled(mc_string_view filename, bool) {
    return musik::core::ffi::valid_text(filename, false) ? MC_ERROR_NOT_FOUND : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_plugin_read_settings(mc_string_view id, mc_plugin_setting_fn read, void*) {
    return read && musik::core::ffi::valid_text(id, false) ? MC_ERROR_NOT_FOUND : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_plugin_write_setting(mc_string_view id, const mc_preference_entry* entry) {
    using namespace musik::core::ffi;
    return entry && valid_text(id, false) && valid_preference_entries(entry, 1)
        ? MC_ERROR_NOT_FOUND
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_output_read_configuration(mc_output_configuration_fn read, void*) {
    return read ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_output_select_driver(mc_string_view driver) {
    return musik::core::ffi::valid_text(driver, false) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_output_select_device(mc_string_view driver, mc_string_view deviceId) {
    return musik::core::ffi::valid_text(driver, false) && musik::core::ffi::valid_text(deviceId)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_output_set_transport(mc_output_transport transport) {
    return (transport == MC_OUTPUT_GAPLESS || transport == MC_OUTPUT_CROSSFADE)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_playback_get_gain_settings(mc_playback_gain_settings* settings) {
    return settings ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_playback_set_preamp(double decibels) {
    return std::isfinite(decibels) && decibels >= -20.0 && decibels <= 20.0
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_playback_set_replay_gain(mc_replay_gain_mode mode) {
    return (mode == MC_REPLAY_GAIN_DISABLED || mode == MC_REPLAY_GAIN_TRACK || mode == MC_REPLAY_GAIN_ALBUM)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_visualizer_list(mc_visualizer_list_fn read, void* data) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&] {
        read(data, nullptr, 0);
        return MC_OK;
    });
}

mc_result MC_CALL mc_visualizer_show(size_t) {
    return MC_ERROR_NOT_FOUND;
}

mc_result MC_CALL mc_visualizer_hide() {
    return MC_ERROR_NOT_FOUND;
}

mc_result MC_CALL mc_visualizer_is_visible(bool* outVisible) {
    if (!outVisible) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *outVisible = false;
    return MC_OK;
}

mc_result MC_CALL mc_visualizer_set_spectrum_enabled(bool) {
    return MC_ERROR_UNSUPPORTED;
}

mc_result MC_CALL mc_visualizer_read_spectrum(float* bins, size_t capacity, uint32_t* sampleRate) {
    return bins && capacity >= MC_VISUALIZER_SPECTRUM_BINS && sampleRate ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_equalizer_get_state(mc_equalizer_state* state) {
    return state && state->struct_size >= sizeof(*state) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_equalizer_set_enabled(bool) {
    return MC_ERROR_UNSUPPORTED;
}

mc_result MC_CALL mc_equalizer_set_gains(const double* gains, size_t count) {
    if (!gains || count != MC_EQUALIZER_BAND_COUNT) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!std::isfinite(gains[i]) || gains[i] < -20.0 || gains[i] > 20.0) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }
    return MC_ERROR_UNSUPPORTED;
}

mc_result MC_CALL mc_path_read(mc_path_kind kind, mc_path_fn read, void*) {
    return read && kind <= MC_PATH_PLUGINS ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_locale_load(mc_string_view directory) {
    return musik::core::ffi::valid_text(directory, false) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_locale_list(mc_locale_text_fn read, void*) {
    return read ? MC_OK : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_locale_read_selected(mc_locale_text_fn read, void*) {
    return read ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_locale_select(mc_string_view locale) {
    return musik::core::ffi::valid_text(locale, false) ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_locale_translate(mc_string_view key, mc_locale_text_fn read, void* userData) {
    if (!read || !musik::core::ffi::valid_text(key, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    read(userData, key);
    return MC_OK;
}

mc_result MC_CALL mc_locale_read_integer(mc_string_view key, int32_t* value) {
    return value && musik::core::ffi::valid_text(key, false)
        ? MC_ERROR_NOT_FOUND : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_log_write(mc_log_level level, mc_string_view tag, mc_string_view text) {
    using namespace musik::core::ffi;
    return level <= MC_LOG_ERROR && valid_text(tag) && valid_text(text)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_lyrics_find_async(mc_context* context, mc_string_view artist, mc_string_view title,
    mc_online_text_fn complete, void*, mc_request_id* request)
{
    if (request) {
        *request = 0;
    }
    using namespace musik::core::ffi;
    return context && complete && request && valid_text(artist) && valid_text(title, false)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_lastfm_request_link_async(mc_context* context, mc_lastfm_link_fn complete, void*, mc_request_id* request) {
    if (request) {
        *request = 0;
    }
    return context && complete && request ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_lastfm_complete_link_async(mc_context* context, mc_string_view token,
    mc_online_text_fn complete, void*, mc_request_id* request)
{
    if (request) {
        *request = 0;
    }
    return context && complete && request && musik::core::ffi::valid_text(token, false)
        ? MC_ERROR_UNSUPPORTED
        : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_lastfm_read_account(mc_online_text_fn read, void*) {
    return read ? MC_ERROR_UNSUPPORTED : MC_ERROR_INVALID_ARGUMENT;
}

mc_result MC_CALL mc_lastfm_unlink(void) {
    return MC_ERROR_UNSUPPORTED;
}
