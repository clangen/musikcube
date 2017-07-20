//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "stdafx.h"

#include "PlaybackOverlays.h"

#include <core/audio/Outputs.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>

#include <vector>

using namespace musik::cube;
using namespace musik::glue::audio;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace cursespp;

static std::vector<std::shared_ptr<IOutput> > plugins;
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

    std::string message = _TSTR("playback_overlay_invalid_transport");
    try {
        message = boost::str(boost::format(message) % outputName);
    }
    catch (...) {
    }

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

void PlaybackOverlays::ShowOutputOverlay(
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

                if (transportType == MasterTransport::Crossfade) {
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

void PlaybackOverlays::ShowTransportOverlay(
    MasterTransport::Type transportType,
    std::function<void(int)> callback)
{
    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("settings_transport_type_gapless"));
    adapter->AddEntry(_TSTR("settings_transport_type_crossfade"));
    adapter->SetSelectable(true);

    size_t selectedIndex = (transportType == MasterTransport::Gapless) ? 0 : 1;

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("playback_overlay_transport_title"))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                int result = (index == 0)
                    ? MasterTransport::Gapless
                    : MasterTransport::Crossfade;

                std::string output = outputs::SelectedOutput()->Name();

                if (result == MasterTransport::Crossfade &&
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
