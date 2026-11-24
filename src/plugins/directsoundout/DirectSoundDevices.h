#pragma once

#include <musikcore/plugin/output.h>

namespace directsound {
    mc_result VisitDevices(mc_output_device_fn read, void* user);
    mc_result ReadDefaultDevice(mc_output_device_fn read, void* user);
}
