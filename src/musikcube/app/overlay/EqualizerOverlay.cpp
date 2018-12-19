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
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/Scrollbar.h>
#include <core/plugin/PluginFactory.h>
#include <cursespp/SingleLineEntry.h>

using namespace cursespp;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;

static const std::string SUPEREQ_PLUGIN_GUID = "6f0ed53b-0f13-4220-9b0a-ca496b6421cc";

static const size_t NO_INDEX = (size_t)-1;

static const std::vector<std::string> BANDS = {
    "65", "92", "131", "185", "262",
    "370", "523", "740", "1047", "1480",
    "2093", "2960", "4186", "5920", "8372",
    "11840", "16744"
};

static const int VERTICAL_PADDING = 2;
static const int MAX_HEIGHT = 8 + (int) BANDS.size();

static std::string formatBandRow(const std::string& band, float value) {
    return u8fmt("%s khz (%0.2f)", band.c_str(), value);
}

EqualizerOverlay::EqualizerOverlay()
: OverlayBase() {
    this->plugin = this->FindPlugin();
    this->prefs = Preferences::ForPlugin(this->plugin->Name());

    this->titleLabel = std::make_shared<TextLabel>();
    this->titleLabel->SetText(_TSTR("equalizer_overlay_title"), text::AlignCenter);
    this->titleLabel->SetBold(true);

    this->enabledCb = std::make_shared<Checkbox>();
    this->enabledCb->SetText(_TSTR("equalizer_overlay_enabled"));
    this->enabledCb->SetChecked(this->prefs->GetBool("enabled", false));

    this->adapter = std::make_shared<BandsAdapter>(prefs);

    this->listView = std::make_shared<ListWindow>(this->adapter);
    this->listView->SetFrameTitle(_TSTR("equalizer_overlay_frequencies"));

    /* add */
    this->AddWindow(this->titleLabel);
    this->AddWindow(this->enabledCb);
    this->AddWindow(this->listView);

    /* focus */
    int order = 0;
    this->enabledCb->SetFocusOrder(order++);
    this->listView->SetFocusOrder(order++);

    /* style */
    style(*this->titleLabel);
    style(*this->enabledCb);
    style(*this->listView);
    this->listView->SetFrameVisible(true);
}

EqualizerOverlay::~EqualizerOverlay() {
}

void EqualizerOverlay::ShowOverlay() {
    App::Overlays().Push(std::make_shared<EqualizerOverlay>());
}

bool EqualizerOverlay::CanScroll(int listViewHeight) {
    return listViewHeight < this->adapter->GetEntryCount();
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

void EqualizerOverlay::Layout() {
    int width = (int)(0.8f * (float) Screen::GetWidth());
    int height = std::min(Screen::GetHeight() - 4, MAX_HEIGHT);
    int y = VERTICAL_PADDING;
    int x = (Screen::GetWidth() / 2) - (width / 2);
    this->MoveAndResize(x, y, width, height);

    int cx = this->GetContentWidth();
    int cy = this->GetContentHeight();
    x = 0;
    y = 0;

    this->titleLabel->MoveAndResize(x, y, cx, 1);
    y += 2;
    this->enabledCb->MoveAndResize(x, y, cx, 1);
    y += 2;
    cy -= 4;
    this->listView->MoveAndResize(x, y, cx, cy);
}

bool EqualizerOverlay::KeyPress(const std::string& key) {
    if (key == "^[" || key == "ESC") { /* esc closes */
        this->Dismiss();
        return true;
    }
    return OverlayBase::KeyPress(key);
}

/* ~~~~~~ BandsAdapter ~~~~~ */

EqualizerOverlay::BandsAdapter::BandsAdapter(EqualizerOverlay::Prefs prefs)
: prefs(prefs) {

}

EqualizerOverlay::BandsAdapter::~BandsAdapter() {
}

size_t EqualizerOverlay::BandsAdapter::GetEntryCount() {
    return BANDS.size();
}

ScrollAdapterBase::EntryPtr EqualizerOverlay::BandsAdapter::GetEntry(cursespp::ScrollableWindow* window, size_t index) {
    const std::string band = BANDS[index];

    auto entry = std::make_shared<SingleLineEntry>(
        formatBandRow(band, prefs->GetDouble(band.c_str(), 1.0)));

    entry->SetAttrs(CURSESPP_DEFAULT_COLOR);
    if (index == window->GetScrollPosition().logicalIndex) {
        entry->SetAttrs(COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM));
    }

    return entry;
}
