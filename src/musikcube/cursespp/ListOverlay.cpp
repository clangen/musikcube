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

#define VERTICAL_PADDING 2
#define DEFAULT_WIDTH 26

ListOverlay::ListOverlay() {
    this->SetFrameVisible(true);
    this->SetFrameColor(CURSESPP_OVERLAY_FRAME);
    this->SetContentColor(CURSESPP_OVERLAY_CONTENT);

    this->autoDismiss = true;

    this->width = this->height = 0;
    this->setWidth = this->setWidthPercent = 0;

    this->listWindow.reset(new ListWindow());
    this->listWindow->SetContentColor(CURSESPP_OVERLAY_CONTENT);
    this->listWindow->SetFocusedContentColor(CURSESPP_OVERLAY_CONTENT);
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
            this->x,
            this->y,
            this->width,
            this->height);

        this->listWindow->MoveAndResize(
            1, /* one pixel padding L and R */
            2, /* below the title, plus an extra space */
            this->GetContentWidth() - 2,
            this->height - 4); /* top and bottom padding + title */

        this->Redraw();
    }
}

ListOverlay& ListOverlay::SetTitle(const std::string& title) {
    this->title = title;
    this->RecalculateSize();
    this->Layout();
    this->Invalidate();
    return *this;
}

ListOverlay& ListOverlay::SetWidth(int width) {
    this->setWidth = width;
    this->setWidthPercent = 0;

    if (this->IsVisible()) {
        this->Layout();
    }

    return *this;
}

ListOverlay& ListOverlay::SetWidthPercent(int percent) {
    this->setWidthPercent = percent;
    this->setWidth = 0;

    if (this->IsVisible()) {
        this->Layout();
    }

    return *this;
}

ListOverlay& ListOverlay::SetAdapter(IScrollAdapterPtr adapter) {
    if (this->adapter != adapter) {
        this->adapter = adapter;
        this->listWindow->SetAdapter(adapter);
    }

    return *this;
}

ListOverlay& ListOverlay::SetAutoDismiss(bool autoDismiss) {
    this->autoDismiss = autoDismiss;
    return *this;
}

ListOverlay& ListOverlay::SetSelectedIndex(size_t index) {
    this->listWindow->SetSelectedIndex(index);

    if (!this->listWindow->IsEntryVisible(index)) {
        this->listWindow->ScrollTo(index);
    }

    return *this;
}

ListOverlay& ListOverlay::SetItemSelectedCallback(ItemSelectedCallback cb) {
    this->itemSelectedCallback = cb;
    return *this;
}

ListOverlay& ListOverlay::SetDeleteKeyCallback(DeleteKeyCallback cb) {
    this->deleteKeyCallback = cb;
    return *this;
}

bool ListOverlay::KeyPress(const std::string& key) {
    if (key == "^[") { /* esc closes */
        this->Dismiss();
        return true;
    }
    else if (key == "KEY_ENTER" || key == " ") {
        if (itemSelectedCallback) {
            itemSelectedCallback(
                this,
                this->adapter,
                listWindow->GetSelectedIndex());
        }
        if (this->autoDismiss) {
            this->Dismiss();
        }
        return true;
    }
    else if (key == "KEY_BACKSPACE" || key == "KEY_DC") {
        if (deleteKeyCallback) {
            deleteKeyCallback(
                this,
                this->adapter,
                listWindow->GetSelectedIndex());

            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}

void ListOverlay::OnVisibilityChanged(bool visible) {
    if (visible) {
        this->LayoutBase::SetFocus(this->listWindow);
        this->Redraw();
    }
}

void ListOverlay::RecalculateSize() {
    if (this->setWidthPercent > 0) {
        int cx = Screen::GetWidth();
        this->width = (int)((this->setWidthPercent / 100.0f) * cx);
    }
    else {
        this->width = this->setWidth > 0 ? this->setWidth : DEFAULT_WIDTH;
    }

    this->height = 4; /* top frame + text + space + bottom frame */
    const int maxHeight = Screen::GetHeight() - 4;

    if (this->adapter) {
        this->height = std::min(
            maxHeight,
            (int) (4 + this->adapter->GetEntryCount()));
    }

    /* constrain to app bounds */
    this->height = std::max(0, std::min(Screen::GetHeight() - 4, this->height));
    this->width = std::max(0, std::min(Screen::GetWidth() - 4, this->width));

    this->y = VERTICAL_PADDING;
    this->x = (Screen::GetWidth() / 2) - (this->width / 2);
}

void ListOverlay::Redraw() {
    if (!this->IsVisible() || this->width <= 0 || this->height <= 0) {
        return;
    }

    WINDOW* c = this->GetContent();

    const int currentX = 1;
    int currentY = 0;

    if (this->title.size()) {
        wmove(c, currentY, currentX);
        wattron(c, A_BOLD);
        checked_wprintw(c, text::Ellipsize(this->title, this->width - 4).c_str());
        wattroff(c, A_BOLD);
        currentY += 2;
    }
}

void ListOverlay::RefreshAdapter() {
    this->listWindow->OnAdapterChanged();
}
