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

#include "PreampOverlay.h"
#include "PlaybackOverlays.h"

#include <core/plugin/PluginFactory.h>
#include <core/i18n/Locale.h>

#include <core/support/PreferenceKeys.h>

#include <cursespp/App.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/Screen.h>

#include <iomanip>
#include <sstream>

using namespace musik;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

#define VERTICAL_PADDING 2
#define DEFAULT_HEIGHT 8
#define DEFAULT_WIDTH 40
#define INPUT_LENGTH 7
#define ARROW std::string("> ")

#define RIGHT(x) (x->GetX() + x->GetWidth())
#define TEXT_WIDTH(x) ((int) u8cols(x->GetText()))
#define INVALID_PREAMP_GAIN -999.9f
#define MIN_PREAMP_GAIN -20.0f
#define MAX_PREAMP_GAIN 20.0f

using Callback = PreampOverlay::Callback;

static std::string getReplayGainMode(std::shared_ptr<Preferences> prefs) {
    ReplayGainMode mode = (ReplayGainMode)prefs->GetInt(
        core::prefs::keys::ReplayGainMode.c_str(),
        (int)ReplayGainMode::Disabled);

    switch (mode) {
        case ReplayGainMode::Disabled: return _TSTR("settings_replay_gain_mode_disabled");
        case ReplayGainMode::Album: return _TSTR("settings_replay_gain_mode_album");
        case ReplayGainMode::Track: return _TSTR("settings_replay_gain_mode_track");
    }

    return _TSTR("settings_replay_gain_mode_disabled");
}

static void showInvalidGainDialog() {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("settings_preamp_invalid_gain_title"))
        .SetMessage(_TSTR("settings_preamp_invalid_gain_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

template <typename T>
static std::string strToFloat(const T value, const int n = 1) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(n) << value;
    return out.str();
}

static float getFloatFromTextInput(TextInput* input) {
    const std::string value = input->GetText();
    if (value.size()) {
        try {
            return std::stof(value);
        }
        catch (...) {
            /* swallow */
        }
    }
    return INVALID_PREAMP_GAIN;
}

PreampOverlay::PreampOverlay(IPlaybackService& playback, Callback callback)
: OverlayBase(), playback(playback)
{
    this->callback = callback;
    this->prefs = Preferences::ForComponent(prefs::components::Playback);
    this->width = this->height = 0;

    this->SetFrameVisible(true);
    this->SetFrameColor(Color::OverlayFrame);
    this->SetContentColor(Color::OverlayContent);

    this->InitViews();
    this->Load();
}

void PreampOverlay::InitViews() {
    /* title */
    this->titleLabel.reset(new TextLabel());
    this->titleLabel->SetText(_TSTR("settings_preamp"), text::AlignCenter);
    this->titleLabel->SetBold(true);

    /* shortcuts */
    this->shortcuts.reset(new ShortcutsWindow());
    this->shortcuts->SetAlignment(text::AlignRight);
    this->shortcuts->AddShortcut("ESC", _TSTR("button_cancel"));
    this->shortcuts->AddShortcut("M-s", _TSTR("button_save"));

    this->shortcuts->SetChangedCallback([this](std::string key) {
        this->KeyPress(key);
    });

    /* preamp gain */
    this->preampLabel.reset(new TextLabel());
    this->preampLabel->SetText(_TSTR("settings_preamp_label"), text::AlignLeft);
    this->preampInput.reset(new TextInput(TextInput::StyleLine));
    this->preampInput->SetTruncate(true);

    /* replay gain */
    std::string mode = getReplayGainMode(this->prefs);
    this->replayGainDropdown.reset(new TextLabel());
    this->replayGainDropdown->SetText(ARROW + _TSTR("settings_replay_gain") + mode);
    this->replayGainDropdown->Activated.connect(this, &PreampOverlay::OnReplayGainPressed);

    /* style 'em */
    style(*this->titleLabel);
    style(*this->preampLabel);
    style(*this->preampInput);
    style(*this->replayGainDropdown);

    /* add 'em */
    this->AddWindow(this->titleLabel);
    this->AddWindow(this->preampLabel);
    this->AddWindow(this->preampInput);
    this->AddWindow(this->replayGainDropdown);
    this->AddWindow(this->shortcuts);

    /* focus order */
    int order = 0;
    this->preampInput->SetFocusOrder(order++);
    this->replayGainDropdown->SetFocusOrder(order++);
}

void PreampOverlay::Layout() {
    this->RecalculateSize();
    this->MoveAndResize(this->x, this->y, this->width, this->height);

    auto clientHeight = this->GetContentHeight();
    auto clientWidth = this->GetContentWidth();

    this->titleLabel->MoveAndResize(0, 0, clientWidth, 1);
    this->shortcuts->MoveAndResize(0, clientHeight - 1, clientWidth, 1);

    int x = 1;
    int y = 2;
    clientWidth -= 2;

    const int preampLabelWidth = TEXT_WIDTH(this->preampLabel);
    this->preampLabel->MoveAndResize(x, y, preampLabelWidth, 1);
    this->preampInput->MoveAndResize(x + preampLabelWidth + 1, y, INPUT_LENGTH, 1);
    y++;

    this->replayGainDropdown->MoveAndResize(x, y, clientWidth, 1);
}

void PreampOverlay::OnReplayGainPressed(cursespp::TextLabel* label) {
    PlaybackOverlays::ShowReplayGainOverlay([this]() {
        this->playback.ReloadOutput();
        this->Load();
    });
}

void PreampOverlay::Show(IPlaybackService& playback, Callback callback) {
    std::shared_ptr<PreampOverlay> overlay(new PreampOverlay(playback, callback));
    App::Overlays().Push(overlay);
}

void PreampOverlay::Load() {
    std::string preampDb = strToFloat(
        prefs->GetDouble(prefs::keys::PreampDecibels.c_str(), 0.0f));

    if (preampDb.size() > INPUT_LENGTH) {
        preampDb = preampDb.substr(0, INPUT_LENGTH);
    }

    this->preampInput->SetText(preampDb);

    std::string mode = getReplayGainMode(this->prefs);
    this->replayGainDropdown->SetText(ARROW + _TSTR("settings_replay_gain") + mode);

    this->Layout();
}

bool PreampOverlay::Save() {
    float preampGain = getFloatFromTextInput(this->preampInput.get());

    if (preampGain > MAX_PREAMP_GAIN || preampGain < MIN_PREAMP_GAIN) {
        showInvalidGainDialog();
        return false;
    }

    this->prefs->SetDouble(prefs::keys::PreampDecibels.c_str(), preampGain);
    this->prefs->Save();

    return true;
}

bool PreampOverlay::KeyPress(const std::string& key) {
    if (key == "^[" || key == "ESC") { /* esc closes */
        this->Dismiss();
        return true;
    }
    else if (key == "M-s") {
        if (Save()) {
            this->Dismiss();
        }
        return true;
    }

    return OverlayBase::KeyPress(key);
}

void PreampOverlay::RecalculateSize() {
    this->width = _DIMEN("server_overlay_width", DEFAULT_WIDTH);
    this->height = std::max(0, std::min(Screen::GetHeight() - 2, DEFAULT_HEIGHT));
    this->width = std::max(0, std::min(Screen::GetWidth(), this->width));
    this->y = VERTICAL_PADDING;
    this->x = (Screen::GetWidth() / 2) - (this->width / 2);
}
