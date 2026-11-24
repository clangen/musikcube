//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include "Outputs.h"
#include "../ffi/common/error.h"
#include "../ffi/common/primitives.h"
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <algorithm>

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::prefs;

namespace musik { namespace core { namespace audio { namespace outputs {
    namespace {
#if defined(__APPLE__)
        constexpr char DefaultOutput[] = "CoreAudio";
#elif defined(WIN32)
        constexpr char DefaultOutput[] = "WASAPI";
#else
        constexpr char DefaultOutput[] = "PulseAudio";
#endif
        std::shared_ptr<Output> findByName(const std::string& name, const std::vector<std::shared_ptr<Output>>& list) {
            for (const auto& output : list) {
                if (output->Name() == name) {
                    return output;
                }
            }
            return {};
        }

        std::shared_ptr<Output> selectedOutput(const std::vector<std::shared_ptr<Output>>& list) {
            const auto prefs = Preferences::ForComponent(components::Playback);
            auto selected = findByName(prefs->GetString(keys::OutputPlugin), list);
            if (!selected) {
                selected = findByName(DefaultOutput, list);
            }
            if (!selected && !list.empty()) {
                selected = list.front();
            }
            return selected ? selected : std::make_shared<Output>();
        }

        mc_string_view text(const std::string& value) {
            return { value.data(), value.size() };
        }
        mc_string_view text(const char* value) {
            return { value, value ? std::char_traits<char>::length(value) : 0 };
        }
        mc_output_device deviceInfo(const Output::Device& device) {
            return { text(device.id), text(device.name) };
        }
        std::string deviceKey(const std::string& pluginId) {
            return "output_device_" + pluginId;
        }
    }

    std::vector<std::shared_ptr<Output>> GetAllOutputs() {
        std::vector<std::shared_ptr<Output>> result;
        const auto plugins = musik::core::plugin::ListPlugins();
        if (plugins.empty()) {
            return result;
        }
        const auto prefs = Preferences::ForComponent(components::Playback);
        for (const auto& info : plugins) {
            const auto factory = musik::core::plugin::FindOutputFactory(info.id);
            if (factory) {
                result.push_back(
                    std::make_shared<Output>(factory, info.id, info.name, prefs->GetString(deviceKey(info.id)),
                        musik::core::plugin::ReadSettingsService(info.name)));
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            return std::string(left->Name()) < right->Name();
        });
        return result;
    }

    std::shared_ptr<Output> SelectedOutput() {
        return selectedOutput(GetAllOutputs());
    }

    void SelectOutput(const std::shared_ptr<Output>& output) {
        if (output) {
            Preferences::ForComponent(components::Playback)->SetString(keys::OutputPlugin, output->Name());
        }
    }

    mc_result Configure(mc_string_view driver, mc_string_view device) {
        if (!ffi::valid_text(driver, false) || !ffi::valid_text(device)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&]() -> mc_result {
            const auto output = findByName(std::string(driver.data, driver.length), GetAllOutputs());
            const std::string selected(device.data ? device.data : "", device.length);
            if (!output || !output->SetDefaultDevice(selected.c_str())) {
                return MC_ERROR_NOT_FOUND;
            }
            /* validate both selections before committing either preference. */
            return Preferences::ForComponent(components::Playback)
                       ->SetValues({ { keys::OutputPlugin, std::string(output->Name()) },
                                       { deviceKey(output->Id()), selected } },
                           true)
                ? MC_OK
                : MC_ERROR_IO;
        });
    }

    mc_result ReadConfiguration(mc_string_view driver, mc_output_configuration_fn read, void* data) {
        if (!read || !ffi::valid_text(driver)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return ffi::exception_barrier(nullptr, [&]() -> mc_result {
            const auto available = GetAllOutputs();
            const auto selected = driver.length ? findByName(std::string(driver.data, driver.length), available)
                                                : selectedOutput(available);
            if (!selected) {
                return MC_ERROR_NOT_FOUND;
            }
            std::vector<mc_string_view> drivers;
            for (const auto& output : available) {
                drivers.push_back(text(output->Name()));
            }
            const auto list = selected->Devices();
            const auto pin = selected->PinnedDeviceId();
            mc_output_device selectedDevice {};
            std::vector<mc_output_device> devices;
            for (const auto& item : list) {
                const auto info = deviceInfo(item);
                devices.push_back(info);
                if (!pin.empty() && pin == item.id) {
                    selectedDevice = info;
                }
            }
            const auto transport = Preferences::ForComponent(components::Playback)->GetInt(keys::Transport, 0);
            const mc_output_configuration configuration { drivers.data(), drivers.size(), text(selected->Name()),
                devices.data(), devices.size(), selectedDevice, static_cast<mc_output_transport>(transport) };
            read(data, &configuration);
            return MC_OK;
        });
    }
} } } }

mc_result MC_CALL mc_output_read_configuration(mc_output_configuration_fn read, void* data) {
    return outputs::ReadConfiguration({ nullptr, 0 }, read, data);
}

mc_result MC_CALL mc_output_read_driver_configuration(
    mc_string_view driver, mc_output_configuration_fn read, void* data)
{
    if (!ffi::valid_text(driver, false) || !read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return outputs::ReadConfiguration(driver, read, data);
}

mc_result MC_CALL mc_output_configure(mc_string_view driver, mc_string_view deviceId) {
    if (!ffi::valid_text(driver, false) || !ffi::valid_text(deviceId)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return outputs::Configure(driver, deviceId);
}

mc_result MC_CALL mc_output_select_driver(mc_string_view driver) {
    if (!ffi::valid_text(driver, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&]() -> mc_result {
        const auto output = outputs::findByName(std::string(driver.data, driver.length), outputs::GetAllOutputs());
        if (!output) {
            return MC_ERROR_NOT_FOUND;
        }
        outputs::SelectOutput(output);
        return MC_OK;
    });
}

mc_result MC_CALL mc_output_select_device(mc_string_view driver, mc_string_view deviceId) {
    if (!ffi::valid_text(driver, false) || !ffi::valid_text(deviceId)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&]() -> mc_result {
        const auto output = outputs::SelectedOutput();
        if (std::string_view(driver.data, driver.length) != output->Name()) {
            return MC_ERROR_NOT_FOUND;
        }
        const std::string id(deviceId.data ? deviceId.data : "", deviceId.length);
        if (!output->SetDefaultDevice(id.c_str())) {
            return MC_ERROR_NOT_FOUND;
        }
        Preferences::ForComponent(components::Playback)->SetString(outputs::deviceKey(output->Id()), id.c_str());
        return MC_OK;
    });
}

mc_result MC_CALL mc_output_set_transport(mc_output_transport transport) {
    if (transport != MC_OUTPUT_GAPLESS && transport != MC_OUTPUT_CROSSFADE) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return ffi::exception_barrier(nullptr, [&] {
        Preferences::ForComponent(components::Playback)->SetInt(keys::Transport, static_cast<int>(transport));
        return MC_OK;
    });
}
