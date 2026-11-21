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

#include <stdafx.h>

#include "PlaybackOverlays.h"

#include <musikcore/output.hpp>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>

#include <vector>

using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace cursespp;

static std::set<std::string> invalidCrossfadeOutputs = { "WaveOut" };

static void showNoOutputPluginsMessage() {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(_TSTR("playback_overlay_no_output_plugins_mesage"))
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

static void showOutputCannotCrossfadeMessage(const std::string& outputName) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    std::string message = u8fmt(
        _TSTR("playback_overlay_invalid_transport"),
        outputName.c_str());

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(message)
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

PlaybackOverlays::PlaybackOverlays() {
}

void PlaybackOverlays::ShowOutputDriverOverlay(
    MasterTransport::Type transportType,
    std::function<void()> callback)
{
    auto output = api::ReadOutputConfiguration();
    if (output.drivers.empty()) {
        showNoOutputPluginsMessage();
        return;
    }

    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    size_t selectedIndex = 0;

    std::shared_ptr<Adapter> adapter(new Adapter());
    for (size_t i = 0; i < output.drivers.size(); i++) {
        const auto& name = output.drivers[i];
        adapter->AddEntry(name);

        if (name == output.selectedDriver) {
            selectedIndex = i;
        }
    }

    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playback_overlay_output_plugins_title"))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback, transportType, drivers = std::move(output.drivers)](ListOverlay*, IScrollAdapterPtr, size_t index) {
                if (index >= drivers.size()) {
                    return;
                }
                const auto& driver = drivers[index];
                if (transportType == MasterTransport::Type::Crossfade && invalidCrossfadeOutputs.count(driver)) {
                    showOutputCannotCrossfadeMessage(driver);
                    return;
                }

                if (api::SelectOutputDriver(driver) == MC_OK && callback) {
                    callback();
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlaybackOverlays::ShowOutputDeviceOverlay(std::function<void()> callback) {
    auto output = api::ReadOutputConfiguration();
    if (output.drivers.empty()) {
        showNoOutputPluginsMessage();
        return;
    }

    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    size_t width = _DIMEN("output_device_overlay_width", 35);
    size_t selectedIndex = 0;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("settings_output_device_default"));

    for (size_t i = 0; i < output.devices.size(); ++i) {
        const auto& device = output.devices[i];
        adapter->AddEntry(device.name);
        width = std::max(width, u8cols(device.name));

        if (device.id == output.selectedDevice.id) {
            selectedIndex = i + 1;
        }
    }

    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playback_overlay_output_device_title"))
        .SetSelectedIndex(selectedIndex)
        .SetWidth(narrow_cast<int>(width))
        .SetItemSelectedCallback(
            [output = std::move(output), callback](ListOverlay*, IScrollAdapterPtr, size_t index) {
                if (index > output.devices.size()) {
                    return;
                }
                const std::string_view id = index == 0 ? std::string_view {} : output.devices[index - 1].id;
                if (api::SelectOutputDevice(output.selectedDriver, id) == MC_OK && callback) {
                    callback();
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlaybackOverlays::ShowTransportOverlay(
    MasterTransport::Type transportType,
    std::function<void(MasterTransport::Type)> callback)
{
    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("settings_transport_type_gapless"));
    adapter->AddEntry(_TSTR("settings_transport_type_crossfade"));
    adapter->SetSelectable(true);

    size_t selectedIndex = (transportType == MasterTransport::Type::Gapless) ? 0 : 1;

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playback_overlay_transport_title"))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                auto result = (index == 0)
                    ? MasterTransport::Type::Gapless
                    : MasterTransport::Type::Crossfade;

                const auto output = api::ReadOutputConfiguration().selectedDriver;

                if (result == MasterTransport::Type::Crossfade &&
                    invalidCrossfadeOutputs.find(output) != invalidCrossfadeOutputs.end())
                {
                    showOutputCannotCrossfadeMessage(output);
                }
                else if (callback) {
                    callback(result);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlaybackOverlays::ShowReplayGainOverlay(std::function<void()> callback) {
    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("settings_replay_gain_mode_disabled"));
    adapter->AddEntry(_TSTR("settings_replay_gain_mode_track"));
    adapter->AddEntry(_TSTR("settings_replay_gain_mode_album"));
    adapter->SetSelectable(true);

    auto prefs = Preferences::ForComponent(core::prefs::components::Playback);

    auto selectedIndex = prefs->GetInt(
        core::prefs::keys::ReplayGainMode.c_str(),
        (int) ReplayGainMode::Disabled);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("settings_replay_gain_title"))
        .SetSelectedIndex((size_t) selectedIndex)
        .SetItemSelectedCallback(
            [callback, selectedIndex, prefs](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                prefs->SetInt(core::prefs::keys::ReplayGainMode.c_str(), (int) index);
                prefs->Save();
                if (selectedIndex != index && callback) {
                    callback();
                }
            });

    cursespp::App::Overlays().Push(dialog);
}
