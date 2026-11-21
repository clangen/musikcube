#ifndef MUSIKCORE_OUTPUT_HPP
#define MUSIKCORE_OUTPUT_HPP

#include <musikcore/context.hpp>
#include <musikcore/output.h>

#include <vector>

namespace musik { namespace core { namespace api {
    /* Owns a device identifier and display name. Copyable and movable. */
    struct OutputDevice {
        std::string id;
        std::string name;
    };

    /* Owns a snapshot of the effective output configuration. Copyable and movable. */
    struct OutputConfiguration {
        std::vector<std::string> drivers;
        std::string selectedDriver;
        std::vector<OutputDevice> devices;
        OutputDevice selectedDevice;
        mc_output_transport transport = MC_OUTPUT_GAPLESS;
    };

    inline OutputConfiguration ReadOutputConfiguration() {
        OutputConfiguration settings;
        const auto result = mc_output_read_configuration([](void* data, const mc_output_configuration* value) {
            auto& output = *static_cast<OutputConfiguration*>(data);
            output.selectedDriver = AsStringView(value->selected_driver);
            output.transport = value->transport;
            output.drivers.reserve(value->driver_count);
            for (size_t i = 0; i < value->driver_count; ++i) {
                output.drivers.emplace_back(AsStringView(value->drivers[i]));
            }

            output.devices.reserve(value->device_count);
            for (size_t i = 0; i < value->device_count; ++i) {
                const auto& device = value->devices[i];
                output.devices.push_back({ std::string(AsStringView(device.id)), std::string(AsStringView(device.name)) });
            }
            output.selectedDevice = {
                std::string(AsStringView(value->selected_device.id)),
                std::string(AsStringView(value->selected_device.name))
            };
        }, &settings);
        if (result != MC_OK && result != MC_ERROR_UNSUPPORTED) {
            throw Error(result);
        }
        return settings;
    }

    inline mc_result SelectOutputDriver(std::string_view name) {
        return mc_output_select_driver({ name.data(), name.size() });
    }

    inline mc_result SelectOutputDevice(std::string_view driver, std::string_view id) {
        return mc_output_select_device({ driver.data(), driver.size() }, { id.data(), id.size() });
    }

    inline mc_result SetOutputTransport(mc_output_transport transport) {
        return mc_output_set_transport(transport);
    }
} } }

#endif
