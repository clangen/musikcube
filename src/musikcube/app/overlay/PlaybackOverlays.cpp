//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/audio/Outputs.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>

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

static std::vector<std::shared_ptr<IOutput> > plugins;
static std::set<std::string> invalidCrossfadeOutputs = { "WaveOut" };

template <typename T>
struct ReleaseDeleter {
    void operator()(T* t) {
        if (t) t->Release();
    }
};

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
    plugins = outputs::GetAllOutputs();

    if (!plugins.size()) {
        showNoOutputPluginsMessage();
        return;
    }

    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    std::string currentOutput = outputs::SelectedOutput()->Name();
    size_t selectedIndex = 0;

    std::shared_ptr<Adapter> adapter(new Adapter());
    for (size_t i = 0; i < plugins.size(); i++) {
        const std::string name = plugins[i]->Name();
        adapter->AddEntry(name);

        if (name == currentOutput) {
            selectedIndex = i;
        }
    }

    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playback_overlay_output_plugins_title"))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback, transportType](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {

                if (transportType == MasterTransport::Type::Crossfade) {
                    std::string output = outputs::GetAllOutputs().at(index)->Name();
                    if (invalidCrossfadeOutputs.find(output) != invalidCrossfadeOutputs.end()) {
                        showOutputCannotCrossfadeMessage(output);
                        return;
                    }
                }

                outputs::SelectOutput(plugins[index]);

                if (callback) {
                    callback();
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void PlaybackOverlays::ShowOutputDeviceOverlay(std::function<void()> callback) {
    auto output = outputs::SelectedOutput();
    if (!output) {
        showNoOutputPluginsMessage();
        return;
    }

    std::string currentDeviceName = _TSTR("settings_output_device_default");

    std::shared_ptr<IDeviceList> deviceList = std::shared_ptr<IDeviceList>(
        output->GetDeviceList(), ReleaseDeleter<IDeviceList>());

    std::shared_ptr<IDevice> device = std::shared_ptr<IDevice>(
        output->GetDefaultDevice(), ReleaseDeleter<IDevice>());

    if (device) {
        currentDeviceName = device->Name();
    }

    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    size_t width = _DIMEN("output_device_overlay_width", 35);
    size_t selectedIndex = 0;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("settings_output_device_default"));

    if (deviceList) {
        for (size_t i = 0; i < deviceList->Count(); i++) {
            const std::string name = deviceList->At(i)->Name();
            adapter->AddEntry(name);

            width = std::max(width, u8cols(name));

            if (name == currentDeviceName) {
                selectedIndex = i + 1;
            }
        }
    }

    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playback_overlay_output_device_title"))
        .SetSelectedIndex(selectedIndex)
        .SetWidth(width)
        .SetItemSelectedCallback(
            [output, deviceList, callback](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == 0) {
                    output->SetDefaultDevice("");
                }
                else {
                    output->SetDefaultDevice(deviceList->At(index - 1)->Id());
                }

                if (callback) {
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

                std::string output = outputs::SelectedOutput()->Name();

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

    auto prefs = Preferences::ForComponent(prefs::components::Playback);

    auto selectedIndex = prefs->GetInt(
        prefs::keys::ReplayGainMode.c_str(),
        (int) ReplayGainMode::Disabled);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("settings_replay_gain_title"))
        .SetSelectedIndex((size_t) selectedIndex)
        .SetItemSelectedCallback(
            [callback, selectedIndex, prefs](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                prefs->SetInt(prefs::keys::ReplayGainMode.c_str(), (int) index);
                prefs->Save();
                if (selectedIndex != index && callback) {
                    callback();
                }
            });

    cursespp::App::Overlays().Push(dialog);
}
