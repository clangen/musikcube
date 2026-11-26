#include "ServerControls.h"

#include "Context.h"

#include <musikcore/equalizer.h>
#include <musikcore/indexer.h>
#include <musikcore/output.h>
#include <musikcore/playback.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {
    using json = nlohmann::json;

    const char* gainName(mc_replay_gain_mode mode) {
        switch (mode) {
            case MC_REPLAY_GAIN_DISABLED: return "disabled";
            case MC_REPLAY_GAIN_TRACK: return "track";
            case MC_REPLAY_GAIN_ALBUM: return "album";
            default: return nullptr;
        }
    }

    bool gainMode(const std::string& text, mc_replay_gain_mode& mode) {
        if (text == "disabled") {
            mode = MC_REPLAY_GAIN_DISABLED;
        }
        else if (text == "track") {
            mode = MC_REPLAY_GAIN_TRACK;
        }
        else if (text == "album") {
            mode = MC_REPLAY_GAIN_ALBUM;
        }
        else {
            return false;
        }
        return true;
    }

    bool getGain(json& output) {
        mc_playback_gain_settings settings {};
        const auto status = mc_playback_get_gain_settings(&settings);
        const auto* mode = gainName(settings.replay_gain);
        if (status != MC_OK || !mode) {
            return false;
        }
        output = {
            { "replaygain_mode", mode },
            { "preamp_gain", settings.preamp_db },
        };
        return true;
    }

    bool setGain(mc_playback* playback, const json& options) {
        const bool hasMode = options.contains("replaygain_mode");
        const bool hasGain = options.contains("preamp_gain");
        mc_replay_gain_mode requestedMode {};
        double requestedGain = 0;
        if (hasMode && !gainMode(options.at("replaygain_mode").get<std::string>(), requestedMode)) {
            return false;
        }
        if (hasGain) {
            requestedGain = options.at("preamp_gain").get<double>();
            if (!std::isfinite(requestedGain)) {
                return false;
            }
            requestedGain = std::clamp(requestedGain, -20.0, 20.0);
        }
        mc_playback_gain_settings current {};
        if (mc_playback_get_gain_settings(&current) != MC_OK) {
            return false;
        }
        bool reload = false;
        if (hasMode && requestedMode != current.replay_gain) {
            if (mc_playback_set_replay_gain(requestedMode) != MC_OK) {
                return false;
            }
            reload = true;
        }
        if (hasGain && requestedGain != current.preamp_db) {
            if (mc_playback_set_preamp(requestedGain) != MC_OK) {
                return false;
            }
            reload = true;
        }
        return !reload || mc_playback_control(playback, MC_PLAYBACK_RELOAD_OUTPUT, 0) == MC_OK;
    }

    bool getEqualizer(json& output) {
        mc_equalizer_state state MC_EQUALIZER_STATE_DEFAULT;
        const auto status = mc_equalizer_get_state(&state);
        if (status != MC_OK) {
            return false;
        }
        json bands = json::object();
        for (const auto& band : state.bands) {
            bands[std::to_string(band.frequency)] = band.gain;
        }
        output = {
            { "enabled", state.enabled },
            { "bands", std::move(bands) },
        };
        return true;
    }

    bool setEqualizer(const json& options) {
        const bool hasEnabled = options.contains("enabled");
        const auto enabled = hasEnabled ? options.at("enabled").get<bool>() : false;
        bool hasBands = false;
        std::array<double, MC_EQUALIZER_BAND_COUNT> gains {};
        if (options.contains("bands")) {
            const auto& bands = options.at("bands");
            if (bands.is_array() && bands.size() == gains.size()) {
                for (size_t i = 0; i < gains.size(); ++i) {
                    gains[i] = bands.at(i).get<double>();
                    if (!std::isfinite(gains[i]) || gains[i] < -20 || gains[i] > 20) {
                        return false;
                    }
                }
                hasBands = true;
            }
        }
        if (hasEnabled && mc_equalizer_set_enabled(enabled) != MC_OK) {
            return false;
        }
        return !hasBands || mc_equalizer_set_gains(gains.data(), gains.size()) == MC_OK;
    }

    struct OutputDevice {
        std::string id;
        std::string name;
    };

    struct OutputDriver {
        std::string name;
        std::vector<OutputDevice> devices;
        bool failed { false };
    };

    struct OutputConfiguration {
        std::vector<OutputDriver> drivers;
        std::string selectedDriver;
        std::string selectedDevice;
        bool failed { false };
    };

    std::string copyText(mc_string_view value) {
        return std::string(value.data ? value.data : "", value.length);
    }

    /* copy callback-scoped views before the engine releases the snapshot. */
    void MC_CALL readOutputDrivers(void* data, const mc_output_configuration* configuration) noexcept {
        auto& output = *static_cast<OutputConfiguration*>(data);
        try {
            output.selectedDriver = copyText(configuration->selected_driver);
            output.selectedDevice = copyText(configuration->selected_device.id);
            output.drivers.reserve(configuration->driver_count);
            for (size_t i = 0; i < configuration->driver_count; ++i) {
                output.drivers.push_back({
                    .name = copyText(configuration->drivers[i]),
                    .devices = {},
                    .failed = false,
                });
            }
        }
        catch (...) {
            output.failed = true;
        }
    }

    void MC_CALL readOutputDevices(void* data, const mc_output_configuration* configuration) noexcept {
        auto& output = *static_cast<OutputDriver*>(data);
        try {
            output.devices.reserve(configuration->device_count);
            for (size_t i = 0; i < configuration->device_count; ++i) {
                const auto& device = configuration->devices[i];
                output.devices.push_back({
                    .id = copyText(device.id),
                    .name = copyText(device.name),
                });
            }
        }
        catch (...) {
            /* propagate allocation failure through the surrounding owner call. */
            output.failed = true;
        }
    }

    bool listOutputs(json& output) {
        OutputConfiguration configuration;
        if (mc_output_read_configuration(readOutputDrivers, &configuration) != MC_OK || configuration.failed) {
            return false;
        }
        for (auto& driver : configuration.drivers) {
            const mc_string_view name {
                .data = driver.name.data(),
                .length = driver.name.size(),
            };
            const auto status = mc_output_read_driver_configuration(name, readOutputDevices, &driver);
            if (driver.failed) {
                return false;
            }
            if (status != MC_OK) {
                /* a device may disappear while other drivers are enumerated. */
                driver.devices.clear();
            }
        }
        json drivers = json::array();
        for (const auto& driver : configuration.drivers) {
            json devices = json::array({ { { "device_name", "default" }, { "device_id", "" } } });
            for (const auto& device : driver.devices) {
                devices.push_back({ { "device_name", device.name }, { "device_id", device.id } });
            }
            drivers.push_back({ { "driver_name", driver.name }, { "devices", std::move(devices) } });
        }
        output = {
            { "all", std::move(drivers) },
            { "selected", {
                { "driver_name", configuration.selectedDriver },
                { "device_id", configuration.selectedDevice },
            } },
        };
        return true;
    }

    bool selectOutput(mc_playback* playback, const json& options) {
        const auto driver = options.value("driver_name", std::string());
        const auto device = options.value("device_id", std::string());
        const mc_string_view driverName {
            .data = driver.data(),
            .length = driver.size(),
        };
        const mc_string_view deviceId {
            .data = device.data(),
            .length = device.size(),
        };
        return mc_output_configure(driverName, deviceId) == MC_OK &&
            mc_playback_control(playback, MC_PLAYBACK_RELOAD_OUTPUT, 0) == MC_OK;
    }

    void MC_CALL readTransport(void* data, const mc_output_configuration* configuration) {
        *static_cast<mc_output_transport*>(data) = configuration->transport;
    }

    bool getTransport(json& output) {
        mc_output_transport transport = MC_OUTPUT_GAPLESS;
        const auto status = mc_output_read_configuration(readTransport, &transport);
        if (status != MC_OK || (transport != MC_OUTPUT_GAPLESS && transport != MC_OUTPUT_CROSSFADE)) {
            return false;
        }
        output = { { "type", transport == MC_OUTPUT_GAPLESS ? "gapless" : "crossfade" } };
        return true;
    }

    bool setTransport(const json& options) {
        const auto type = options.value("type", std::string());
        if (type.empty()) {
            return true;
        }
        if (type != "gapless" && type != "crossfade") {
            return false;
        }
        const auto transport = type == "gapless" ? MC_OUTPUT_GAPLESS : MC_OUTPUT_CROSSFADE;
        return mc_output_set_transport(transport) == MC_OK;
    }
}

bool ExecuteServerControl(mc_playback* playback, std::string_view operation,
    const json& options, json& output)
{
    try {
        output = json::object();
        if (operation == "list_output_drivers") {
            return listOutputs(output);
        }
        if (operation == "set_default_output_driver") {
            return selectOutput(playback, options);
        }
        if (operation == "get_gain_settings") {
            return getGain(output);
        }
        if (operation == "set_gain_settings") {
            return setGain(playback, options);
        }
        if (operation == "get_equalizer_settings") {
            return getEqualizer(output);
        }
        if (operation == "set_equalizer_settings") {
            return setEqualizer(options);
        }
        if (operation == "get_transport_type") {
            return getTransport(output);
        }
        if (operation == "set_transport_type") {
            return setTransport(options);
        }
        return false;
    }
    catch (...) {
        return false;
    }
}

mc_result ScheduleServerIndexer(Context& context, const json& options,
    std::function<void(mc_result)> completion)
{
    if (!context.indexer || context.indexer->struct_size < sizeof(mc_plugin_indexer_service) ||
        !context.indexer->schedule_async) {
        return MC_ERROR_UNSUPPORTED;
    }
    const auto scan = options.value("type", std::string("reindex")) == "rebuild"
        ? MC_INDEXER_REBUILD : MC_INDEXER_SCAN_LOCAL;
    auto* pending = new std::function<void(mc_result)>(
        [&context, completion = std::move(completion)](mc_result result) {
            try {
                completion(result);
            }
            catch (...) {
                context.Log(MC_LOG_ERROR, "ServerControls", "indexer completion failed");
                std::terminate();
            }
        });
    const auto status = context.indexer->schedule_async(
        context.indexer->context, scan, [](void* data, mc_result result) noexcept {
            std::unique_ptr<std::function<void(mc_result)>> callback(
                static_cast<std::function<void(mc_result)>*>(data));
            (*callback)(result);
        }, pending);
    if (status != MC_OK) {
        delete pending;
    }
    return status;
}
