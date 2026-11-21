#include <musikcore/output.h>

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
