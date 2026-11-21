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

#include "EqualizerOverlay.h"

#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/Scrollbar.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <musikcore/support/Messages.h>


using namespace cursespp;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;

static const int VERTICAL_PADDING = 2;
static const int MAX_HEIGHT = 7 + MC_EQUALIZER_BAND_COUNT;
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
    int remain = narrow_cast<int>(width - (u8cols(leftText) + u8cols(rightText)) + 1);

    /* pad the area between the left and right text, if necessary */
    return text::Align(leftText, text::AlignLeft, u8cols(leftText) + remain) + rightText;
}

EqualizerOverlay::EqualizerOverlay(api::Context& musikcore, const mc_equalizer_state& state)
: OverlayBase()
, musikcore(musikcore)
, state(state)
{
    this->enabledCb = std::make_shared<Checkbox>();
    this->enabledCb->SetText(_TSTR("equalizer_overlay_enabled"));
    this->enabledCb->SetAlignment(text::AlignCenter);
    this->enabledCb->SetBold(true);
    this->enabledCb->SetChecked(this->state.enabled);
    this->enabledCb->CheckChanged.connect(this, &EqualizerOverlay::OnEnabledChanged);

    this->adapter = std::make_shared<BandsAdapter>(this->state);

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

    const auto status = api::SetEqualizerChangedCallback(musikcore, [](void* data) {
        static_cast<EqualizerOverlay*>(data)->Refresh();
    }, this);
    if (status != MC_OK) {
        throw api::Error(status);
    }
}

EqualizerOverlay::~EqualizerOverlay() {
    api::SetEqualizerChangedCallback(this->musikcore, nullptr);
}

void EqualizerOverlay::ShowOverlay(api::Context& musikcore) {
    if (const auto state = api::ReadEqualizer()) {
        auto overlay = std::make_shared<EqualizerOverlay>(musikcore, *state);
        App::Overlays().Push(overlay);
    }
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
    api::SetEqualizerEnabled(checked);
    this->Refresh();
}

bool EqualizerOverlay::KeyPress(const std::string& key) {
    auto& keys = NavigationKeys();

    if (key == "^[" || key == "ESC") { /* esc closes */
        this->Dismiss();
        return true;
    }
    else if (key == "z") {
        api::SetEqualizerGains({});
        this->Refresh();
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

void EqualizerOverlay::Refresh() {
    if (const auto updated = api::ReadEqualizer()) {
        this->state = *updated;
        this->enabledCb->SetChecked(this->state.enabled);
        this->listView->OnAdapterChanged();
    }
}

void EqualizerOverlay::UpdateSelectedBand(double delta) {
    const auto index = this->listView->GetSelectedIndex();
    if (index >= MC_EQUALIZER_BAND_COUNT) {
        return;
    }

    api::EqualizerGains gains;
    for (size_t i = 0; i < gains.size(); ++i) {
        gains[i] = this->state.bands[i].gain;
    }
    gains[index] = std::clamp(gains[index] + delta, -20.0, 20.0);
    api::SetEqualizerGains(gains);
    this->Refresh();
}

/* ~~~~~~ BandsAdapter ~~~~~ */

EqualizerOverlay::BandsAdapter::BandsAdapter(const mc_equalizer_state& state)
: state(state) {

}

EqualizerOverlay::BandsAdapter::~BandsAdapter() {
}

size_t EqualizerOverlay::BandsAdapter::GetEntryCount() {
    return MC_EQUALIZER_BAND_COUNT;
}

ScrollAdapterBase::EntryPtr EqualizerOverlay::BandsAdapter::GetEntry(cursespp::ScrollableWindow* window, size_t index) {
    const auto& band = this->state.bands[index];

    auto entry = std::make_shared<SingleLineEntry>(formatBandRow(
        window->GetContentWidth(),
        std::to_string(band.frequency),
        band.gain));

    entry->SetAttrs(Color::Default);
    if (index == window->GetScrollPosition().logicalIndex) {
        entry->SetAttrs(Color::ListItemHighlighted);
    }

    return entry;
}
