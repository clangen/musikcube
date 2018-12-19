//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "EqualizerOverlay.h"
#include <cursespp/Screen.h>
#include <core/plugin/PluginFactory.h>

using namespace cursespp;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;

static const std::string SUPEREQ_PLUGIN_GUID = "6f0ed53b-0f13-4220-9b0a-ca496b6421cc";

static std::string formatBandRow(const std::string& band, float value) {
    return band + "khz";
}

static int calculateWidth() {
    return (int)(0.8f * Screen::GetWidth());
}

EqualizerOverlay::EqualizerOverlay()
: OverlayBase() {
    this->plugin = this->FindPlugin();
    this->prefs = Preferences::ForPlugin(this->plugin->Name());
}

void EqualizerOverlay::ShowOverlay() {
}

std::shared_ptr<IPlugin> EqualizerOverlay::FindPlugin() {
    std::shared_ptr<IPlugin> result;
    using Deleter = PluginFactory::ReleaseDeleter<IPlugin>;
    PluginFactory::Instance().QueryInterface<IPlugin, Deleter>(
        "GetPlugin",
        [&result](IPlugin* unused, std::shared_ptr<IPlugin> plugin, const std::string& fn) {
            if (std::string(plugin->Guid()) == SUPEREQ_PLUGIN_GUID) {
                result = plugin;
            }
        });
    return result;
}
