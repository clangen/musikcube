#include <musikcore/output.h>
#include <musikcore/equalizer.h>
#include <musikcore/playback.h>

#include <cmath>

#include "../common/primitives.h"

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
