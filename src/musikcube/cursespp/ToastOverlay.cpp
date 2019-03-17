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
#include <cursespp/App.h>
#include <cursespp/ToastOverlay.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/Text.h>

static const int TOAST_MESSAGE_HIDE = 1000;

using namespace cursespp;
using namespace musik::core::runtime;

void ToastOverlay::Show(const std::string& text, int durationMs) {
    std::shared_ptr<ToastOverlay> overlay(new ToastOverlay(text, durationMs));
    App::Overlays().Push(overlay);
}

ToastOverlay::ToastOverlay(const std::string& text, long durationMs) {
    this->title = text;
    this->durationMs = durationMs;
    this->width = this->height = 0;
    this->ticking = false;
    this->SetFrameVisible(true);
    this->SetFrameColor(Color::OverlayFrame);
    this->SetFocusedFrameColor(Color::OverlayFrame);
    this->SetContentColor(Color::OverlayContent);
    this->SetFocusedContentColor(Color::OverlayContent);
}

ToastOverlay::~ToastOverlay() {
}

void ToastOverlay::Layout() {
    this->RecalculateSize();

    if (this->width > 0 && this->height > 0) {
        this->MoveAndResize(this->x, this->y, this->width, this->height);
        this->Redraw();
    }
}

bool ToastOverlay::KeyPress(const std::string& key) {
    this->Dismiss(); /* any key closes */
    this->Remove(TOAST_MESSAGE_HIDE);
    return true;
}

void ToastOverlay::OnVisibilityChanged(bool visible) {
    if (visible && !ticking) {
        if (this->durationMs >= 0) {
            this->Post(TOAST_MESSAGE_HIDE, 0, 0, this->durationMs);
        }
        this->ticking = true;
    }
}

void ToastOverlay::ProcessMessage(IMessage &message) {
    if (message.Type() == TOAST_MESSAGE_HIDE) {
        this->Dismiss();
    }
}

void ToastOverlay::RecalculateSize() {
    int cols = (int) u8cols(this->title);
    this->width = std::min(cols + 4, (Screen::GetWidth() * 4) / 5);
    this->titleLines = text::BreakLines(this->title, this->width - 4);
    this->height = std::min((int) this->titleLines.size() + 2, Screen::GetHeight() - 4);
    this->x = (Screen::GetWidth() / 2) - (this->width / 2);
    this->y = 2;
}

void ToastOverlay::OnRedraw() {
     if (!this->IsVisible() || this->width <= 0 || this->height <= 0) {
         return;
     }

     WINDOW* c = this->GetContent();

     for (int i = 0; i < (int) this->titleLines.size(); i++) {
         wmove(c, i, 1);
         checked_wprintw(c, text::Ellipsize(this->titleLines[i], this->width - 4).c_str());
     }
}
