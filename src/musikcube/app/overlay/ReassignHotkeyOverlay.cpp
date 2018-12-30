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
#include "ReassignHotkeyOverlay.h"
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <algorithm>

using namespace cursespp;
using namespace musik::cube;

#define VERTICAL_PADDING 2
#define DEFAULT_HEIGHT 10
#define DEFAULT_WIDTH 35

void ReassignHotkeyOverlay::Show(Hotkeys::Id id, Callback callback) {
    using T = ReassignHotkeyOverlay;
    auto overlay = std::shared_ptr<T>(new T(id, callback));
    App::Overlays().Push(overlay);
}

void ReassignHotkeyOverlay::Layout() {
    this->RecalculateSize();
    this->MoveAndResize(this->x, this->y, this->width, this->height);

    auto clientHeight = this->height - 2;
    auto clientWidth = this->width - 2;

    auto name = Hotkeys::Name(this->id);
    this->hotkeyLabel->SetText(name, text::AlignCenter);

    this->titleLabel->MoveAndResize(0, 0, clientWidth, 1);
    this->hotkeyLabel->MoveAndResize(1, 2, clientWidth - 2, 3);
    this->hotkeyInput->MoveAndResize(1, 3, clientWidth - 2, 3);
    this->shortcuts->MoveAndResize(0, clientHeight - 1, clientWidth, 1);
}

bool ReassignHotkeyOverlay::KeyPress(const std::string& key) {
    if (key == "^[" || key == "ESC") { /* esc closes */
        this->Dismiss();
        return true;
    }
    else if (key == "KEY_ENTER") {
        auto current = this->hotkeyInput->GetText();
        if (u8len(current)) {
            callback(current);
            this->Dismiss();
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}

ReassignHotkeyOverlay::ReassignHotkeyOverlay(Hotkeys::Id id, Callback callback)
: id(id), callback(callback) {
    LayoutBase();
    this->InitViews();
}

void ReassignHotkeyOverlay::RecalculateSize() {
    auto preferredWidth = _DIMEN("reassign_hotkey_overlay_width", DEFAULT_WIDTH);
    auto name = Hotkeys::Name(this->id);

    this->width = std::max(int(u8cols(name) + 4), preferredWidth);
    this->width = std::max(0, std::min(Screen::GetWidth(), this->width));
    this->height = std::max(0, std::min(Screen::GetHeight() - 2, DEFAULT_HEIGHT));
    this->y = VERTICAL_PADDING;
    this->x = (Screen::GetWidth() / 2) - (this->width / 2);
}

void ReassignHotkeyOverlay::InitViews() {
    this->SetFrameVisible(true);
    this->SetFrameColor(Color::OverlayFrame);
    this->SetContentColor(Color::OverlayContent);

    this->titleLabel = std::make_shared<TextLabel>();
    style(*this->titleLabel);
    this->titleLabel->SetText(_TSTR("hotkeys_reassign_overlay_title"), text::AlignCenter);
    this->titleLabel->SetBold(true);

    this->hotkeyLabel = std::make_shared<TextLabel>();
    style(*this->hotkeyLabel);

    this->hotkeyInput = std::make_shared<TextInput>(
        TextInput::Style::StyleBox, TextInput::InputRaw);

    this->hotkeyInput->SetRawKeyBlacklist({ "KEY_ENTER", "^[" });
    this->hotkeyInput->SetFocusOrder(0);
    this->hotkeyInput->SetFocusedFrameColor(Color::OverlayTextInputFrame);
    this->hotkeyInput->SetText(Hotkeys::Get(this->id));
    this->hotkeyInput->SetFocusedContentColor(Color::OverlayContent);

    this->shortcuts = std::make_shared<ShortcutsWindow>();
    this->shortcuts->SetAlignment(text::AlignRight);
    this->shortcuts->AddShortcut("ESC", _TSTR("button_cancel"));
    this->shortcuts->AddShortcut("ENTER", _TSTR("button_save"));

    this->AddWindow(this->titleLabel);
    this->AddWindow(this->hotkeyLabel);
    this->AddWindow(this->hotkeyInput);
    this->AddWindow(this->shortcuts);

    this->Layout();
}