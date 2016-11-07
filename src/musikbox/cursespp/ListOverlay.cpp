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
#include "ListOverlay.h"
#include "Colors.h"
#include "Screen.h"
#include "Text.h"

using namespace cursespp;

#define HORIZONTAL_PADDING 4
#define VERTICAL_PADDING 2

#define DISMISS() \
    OverlayStack* overlays = this->GetOverlayStack(); \
    if (overlays) { \
        overlays->Remove(this); \
    }

ListOverlay::ListOverlay() {
    this->SetFrameVisible(true);
    this->SetFrameColor(CURSESPP_OVERLAY_FRAME);
    this->SetContentColor(CURSESPP_OVERLAY_BACKGROUND);

    this->width = this->height = 0;

    this->listWindow.reset(new ListWindow());
    this->listWindow->SetFrameVisible(false);
    this->listWindow->SetFocusOrder(0);
    this->AddWindow(this->listWindow);
}

ListOverlay::~ListOverlay() {
}

void ListOverlay::Layout() {
    this->RecalculateSize();

    if (this->width > 0 && this->height > 0) {
        this->MoveAndResize(
            HORIZONTAL_PADDING,
            VERTICAL_PADDING,
            this->width,
            this->height);

        int y = VERTICAL_PADDING + 3; /* below the border + title */
        int cy = this->height - 4; /* top and bottom padding + title */

        this->listWindow->MoveAndResize(
            HORIZONTAL_PADDING + 1, y,
            this->GetContentWidth(), cy);
    }
}

ListOverlay& ListOverlay::SetTitle(const std::string& title) {
    this->title = title;
    this->RecalculateSize();
    this->Layout();
    this->Repaint();
    return *this;
}

ListOverlay& ListOverlay::SetAdapter(IScrollAdapterPtr adapter) {
    if (this->adapter != adapter) {
        this->adapter = adapter;
        this->listWindow->SetAdapter(adapter.get());
    }

    return *this;
}

ListOverlay& ListOverlay::SetItemSelectedCallback(ItemSelectedCallback cb) {
    this->itemSelectedCallback = cb;
    return *this;
}

bool ListOverlay::KeyPress(const std::string& key) {
    if (key == "^[") { /* esc closes */
        DISMISS();
        return true;
    }
    else if (key == "KEY_ENTER") {
        if (itemSelectedCallback) {
            itemSelectedCallback(
                this->adapter,
                listWindow->GetSelectedIndex());
        }
        DISMISS();
        return true;
    }

    return LayoutBase::KeyPress(key);
}

void ListOverlay::OnVisibilityChanged(bool visible) {
    if (visible) {
        this->SetFocus(this->listWindow);
        this->Redraw();
    }
}

void ListOverlay::RecalculateSize() {
    this->width = 20;
    this->height = 10;

    //int lastWidth = this->width;

    //this->width = std::max(0, Screen::GetWidth() - (HORIZONTAL_PADDING * 2));

    //if (lastWidth != this->width) {
    //    /* 4 here: 2 for the frame padding (left/right), then two for the
    //    inner content padding so things aren't bunched up (left/right) */
    //    messageLines = text::BreakLines(this->message, this->width - 4);
    //}

    //this->height = 0; /* top padding */
    //this->height += (this->title.size()) ? 2 : 0;
    //this->height += (this->messageLines.size()) ? messageLines.size() + 1 : 0;
    //this->height += 1; /* shortcuts */

    ///* ensure the overlay doesn't exceed the height of the screen,
    //or things may get crashy. normally this will be done for us automatically
    //in Window, but because we're free-floating we need to do it manually here. */
    //int top = this->GetY();
    //int bottom = top + this->height + VERTICAL_PADDING;
    //int screenHeight = Screen::GetHeight();
    //if (bottom > screenHeight) {
    //    this->height = screenHeight - top - VERTICAL_PADDING;
    //}

    //int left = this->GetX();
    //int right = left + this->width + HORIZONTAL_PADDING;
    //int screenWidth = Screen::GetWidth();
    //if (right > screenWidth) {
    //    this->width = screenWidth - left - HORIZONTAL_PADDING;
    //}
}

void ListOverlay::Redraw() {
    if (this->width <= 0 || this->height <= 0) {
        return;
    }

    WINDOW* c = this->GetContent();

    const int currentX = 1;
    int currentY = 0;

    if (this->title.size()) {
        wmove(c, currentY, currentX);
        wattron(c, A_BOLD);
        wprintw(c, text::Ellipsize(this->title, this->width - 4).c_str());
        wattroff(c, A_BOLD);
        currentY += 2;
    }
}