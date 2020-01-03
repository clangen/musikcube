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

#include "EqualizerOverlay.h"

#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/Scrollbar.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <core/support/Messages.h>
#include <core/plugin/PluginFactory.h>

#include <app/util/Messages.h>

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
    "11840", "16744", "22000"
};

static const int VERTICAL_PADDING = 2;
static const int MAX_HEIGHT = 7 + (int) BANDS.size();
static const int DEFAULT_WIDTH = 46;
static const int TRACK_WIDTH = 21;
static const int DB_LABEL_WIDTH = 6; /* "-XY dB" */

static std::string formatDbLabel(double db) {
    return u8fmt("%d dB", (int)round(db));
}

static std::string formatBandRow(size_t width, const std::string& band, double db) {
    width -= 2; /* left/right padding */
    int leftWidth = 10; /* "16744 hz " */

    /* frequency */
    std::string leftText = u8fmt(" %s hz", band.c_str());
    leftText = text::Align(leftText, text::AlignRight, leftWidth);

    /* track */
    double percent = (db + 20.0) / 40.0;
    std::string trackText;
    size_t thumbOffset = std::min(TRACK_WIDTH - 1, (int) (percent * TRACK_WIDTH));
    for (int i = 0; i < TRACK_WIDTH; i++) {
        trackText += (i == thumbOffset) ? "■" : "─";
    }

    /* db */
    std::string dbText = text::Align(formatDbLabel(db), text::AlignRight, DB_LABEL_WIDTH);

    /* track + db */
    std::string rightText = u8fmt(" %s %s", trackText.c_str(), dbText.c_str());

    /* TODO: i'm dumb and it's late; we shouldn't need the `+ 1` here, there's
    another calculation error that i'm currently blind to. */
    int remain = (int) width - (u8cols(leftText) + u8cols(rightText)) + 1;

    /* pad the area between the left and right text, if necessary */
    return text::Align(leftText, text::AlignLeft, u8cols(leftText) + remain) + rightText;
}

EqualizerOverlay::EqualizerOverlay()
: OverlayBase() {
    this->plugin = this->FindPlugin();
    this->prefs = Preferences::ForPlugin(this->plugin->Name());

    this->enabledCb = std::make_shared<Checkbox>();
    this->enabledCb->SetText(_TSTR("equalizer_overlay_enabled"));
    this->enabledCb->SetAlignment(text::AlignCenter);
    this->enabledCb->SetBold(true);
    this->enabledCb->SetChecked(this->prefs->GetBool("enabled", false));
    this->enabledCb->CheckChanged.connect(this, &EqualizerOverlay::OnEnabledChanged);

    this->adapter = std::make_shared<BandsAdapter>(prefs);

    this->listView = std::make_shared<ListWindow>(this->adapter);
    this->listView->SetFrameTitle(_TSTR("equalizer_overlay_frequencies"));

    this->shortcuts = std::make_shared<ShortcutsWindow>();
    // this->shortcuts->AddShortcut("l", _TSTR("equalizer_button_load"));
    // this->shortcuts->AddShortcut("s", _TSTR("equalizer_button_save"));
    this->shortcuts->AddShortcut("z", _TSTR("equalizer_button_zero"));
    this->shortcuts->AddShortcut("ESC", _TSTR("button_close"));
    this->shortcuts->SetAlignment(text::AlignRight);

    /* add */
    this->AddWindow(this->enabledCb);
    this->AddWindow(this->listView);
    this->AddWindow(this->shortcuts);

    /* focus */
    int order = 0;
    this->enabledCb->SetFocusOrder(order++);
    this->listView->SetFocusOrder(order++);

    /* style */
    style(*this->enabledCb);
    style(*this->listView);
    this->listView->SetFrameVisible(true);
}

EqualizerOverlay::~EqualizerOverlay() {
    MessageQueue().UnregisterForBroadcasts(this);
}

void EqualizerOverlay::ShowOverlay() {
    auto overlay = std::make_shared<EqualizerOverlay>();
    App::Overlays().Push(overlay);
    MessageQueue().RegisterForBroadcasts(overlay);
}

std::shared_ptr<IPlugin> EqualizerOverlay::FindPlugin() {
    return PluginFactory::Instance().QueryGuid(SUPEREQ_PLUGIN_GUID);
}

void EqualizerOverlay::Layout() {
    int width = _DIMEN("equalizer_overlay_width", DEFAULT_WIDTH);
    int height = std::min(Screen::GetHeight() - 4, MAX_HEIGHT);
    int y = VERTICAL_PADDING;
    int x = (Screen::GetWidth() / 2) - (width / 2);
    this->MoveAndResize(x, y, width, height);

    int cx = this->GetContentWidth();
    int cy = this->GetContentHeight();
    x = 0;
    y = 0;

    this->enabledCb->MoveAndResize(x, y, cx, 1);
    y += 2;
    cy -= 2;
    this->listView->MoveAndResize(x, y, cx, cy - 1);

    cy = this->GetContentHeight();
    this->shortcuts->MoveAndResize(0, cy - 1, cx, 1);
}

void EqualizerOverlay::OnEnabledChanged(cursespp::Checkbox* cb, bool checked) {
    this->prefs->SetBool("enabled", checked);
    this->NotifyAndRedraw();
}

bool EqualizerOverlay::KeyPress(const std::string& key) {
    auto& keys = NavigationKeys();

    if (key == "^[" || key == "ESC") { /* esc closes */
        this->Dismiss();
        return true;
    }
    else if (key == "z") {
        for (auto band : BANDS) {
            this->prefs->SetDouble(band.c_str(), 0.0);
        }
        this->NotifyAndRedraw();
    }
    else if (this->GetFocus() == this->listView) {
        if (keys.Left(key)) {
            this->UpdateSelectedBand(-1.0);
            return true;
        }
        else if (keys.Right(key)) {
            this->UpdateSelectedBand(1.0);
            return true;
        }
    }
    return OverlayBase::KeyPress(key);
}

void EqualizerOverlay::NotifyAndRedraw() {
    this->listView->OnAdapterChanged();
    this->plugin->Reload();
}

void EqualizerOverlay::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == message::UpdateEqualizer) {
        this->plugin->Reload();
    }
    else if (message.Type() == musik::core::message::EqualizerUpdated) {
        this->enabledCb->SetChecked(this->prefs->GetBool("enabled", false));
        this->listView->OnAdapterChanged();
    }
}

void EqualizerOverlay::UpdateSelectedBand(double delta) {
    const std::string band = BANDS[this->listView->GetSelectedIndex()];
    double existing = this->prefs->GetDouble(band.c_str(), 0.0);
    double updated = std::min(20.0, std::max(-20.0, existing + delta));
    this->prefs->SetDouble(band.c_str(), updated);
    this->NotifyAndRedraw();
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

    auto entry = std::make_shared<SingleLineEntry>(formatBandRow(
        window->GetContentWidth(),
        band,
        prefs->GetDouble(band.c_str(), 0.0)));

    entry->SetAttrs(Color::Default);
    if (index == window->GetScrollPosition().logicalIndex) {
        entry->SetAttrs(Color::ListItemHighlighted);
    }

    return entry;
}
