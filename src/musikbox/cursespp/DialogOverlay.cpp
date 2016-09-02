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

#include "DialogOverlay.h"
#include "Colors.h"
#include "Screen.h"
#include "Text.h"

using namespace cursespp;

#define HORIZONTAL_PADDING 4
#define VERTICAL_PADDING 2

DialogOverlay::DialogOverlay() {
    this->SetContentColor(CURSESPP_OVERLAY_BACKGROUND);
    this->width = this->height = 0;
}

void DialogOverlay::Layout() { 
    this->RecalculateSize();

    this->MoveAndResize(
        HORIZONTAL_PADDING, 
        VERTICAL_PADDING, 
        this->width,
        this->height);

    this->Redraw();
}

DialogOverlay& DialogOverlay::SetTitle(const std::string& title) {
    this->title = title;
    this->RecalculateSize();
    this->Repaint();
    return *this;
}

DialogOverlay& DialogOverlay::SetMessage(const std::string& message) {
    this->message = message;
    this->width = 0; /* implicitly triggers a new BreakLines() */
    this->RecalculateSize();
    this->Repaint();
    return *this;
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

    this->height = 1; /* top padding */
    this->height += (this->title.size()) ? 2 : 0;
    this->height += (this->messageLines.size()) ? messageLines.size() + 1 : 0;
    this->height += 1; /* shortcuts */
}

void DialogOverlay::Redraw() {
    WINDOW* c = this->GetContent();

    const int currentX = 1;
    int currentY = 1;

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