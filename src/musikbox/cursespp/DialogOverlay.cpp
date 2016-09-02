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

#include <stdafx.h>
#include "DialogOverlay.h"
#include "Colors.h"
#include "Screen.h"
#include "Text.h"

using namespace cursespp;

#define HORIZONTAL_PADDING 4
#define VERTICAL_PADDING 2

DialogOverlay::DialogOverlay() {
    this->SetFrameVisible(true);
    this->SetFrameColor(CURSESPP_OVERLAY_FRAME);
    this->SetContentColor(CURSESPP_OVERLAY_BACKGROUND);

    this->width = this->height = 0;
    this->autoDismiss = true;

    this->shortcuts.reset(new ShortcutsWindow());
    this->AddWindow(this->shortcuts);
}

DialogOverlay::~DialogOverlay() {
}

void DialogOverlay::Layout() {
    this->RecalculateSize();

    if (this->width > 0 && this->height > 0) {
        this->MoveAndResize(
            HORIZONTAL_PADDING,
            VERTICAL_PADDING,
            this->width,
            this->height + 2);

        this->shortcuts->MoveAndResize(
            HORIZONTAL_PADDING + 1,
            VERTICAL_PADDING + this->height,
            this->GetContentWidth(),
            1);

        this->Redraw();
    }
}

DialogOverlay& DialogOverlay::SetTitle(const std::string& title) {
    this->title = title;
    this->RecalculateSize();
    this->Layout();
    this->Repaint();
    return *this;
}

DialogOverlay& DialogOverlay::SetMessage(const std::string& message) {
    this->message = message;
    this->width = 0; /* implicitly triggers a new BreakLines() */
    this->RecalculateSize();
    this->Layout();
    this->Repaint();
    return *this;
}

DialogOverlay& DialogOverlay::SetAutoDismiss(bool dismiss) {
    this->autoDismiss = dismiss;
    return *this;
}

DialogOverlay& DialogOverlay::AddButton(
    const std::string& rawKey,
    const std::string& key,
    const std::string& caption,
    ButtonCallback callback)
{
    this->shortcuts->AddShortcut(key, caption);
    this->buttons[rawKey] = callback;
    this->Layout();
    this->Repaint();
    return *this;
}

bool DialogOverlay::KeyPress(const std::string& key) {
    auto it = this->buttons.find(key);

    if (it != this->buttons.end()) {
        ButtonCallback cb = it->second;

        if (cb) {
            cb(key);
        }

        if (this->autoDismiss) {
            OverlayStack* overlays = this->GetOverlayStack();
            if (overlays) {
                overlays->Remove(this);
            }
        }

        return true;
    }

    return LayoutBase::KeyPress(key);
}

void DialogOverlay::OnVisibilityChanged(bool visible) {
    if (visible) {
        this->Redraw();
    }
}

void DialogOverlay::RecalculateSize() {
    int lastWidth = this->width;

    this->width = std::max(0, Screen::GetWidth() - (HORIZONTAL_PADDING * 2));

    if (lastWidth != this->width) {
        /* the "-2" is for left and right padding */
        messageLines = text::BreakLines(this->message, this->width - 2);
    }

    this->height = 0; /* top padding */
    this->height += (this->title.size()) ? 2 : 0;
    this->height += (this->messageLines.size()) ? messageLines.size() + 1 : 0;
    this->height += 1; /* shortcuts */
}

void DialogOverlay::Redraw() {
    if (this->width <= 0 || this->height <= 0) {
        return;
    }

    WINDOW* c = this->GetContent();

    const int currentX = 1;
    int currentY = 0;

    if (this->title.size()) {
        wmove(c, currentY, currentX);
        wattron(c, A_BOLD);
        wprintw(c, text::Ellipsize(this->title, this->width - 2).c_str());
        wattroff(c, A_BOLD);
        currentY += 2;
    }

    if (this->message.size()) {
        for (size_t i = 0; i < messageLines.size(); i++) {
            wmove(c, currentY, currentX);
            wprintw(c, this->messageLines.at(i).c_str());
            ++currentY;
        }
    }
}