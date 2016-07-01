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

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/MessageQueue.h>
#include <cursespp/Message.h>
#include <cursespp/Text.h>

#include "Checkbox.h"

using namespace cursespp;

#define UNCHECKED std::string("□")
#define CHECKED std::string("✓")

Checkbox::Checkbox()
: Window()
, checked(false)
, focused(false) {
    this->SetFrameVisible(false);
}

Checkbox::~Checkbox() {
}

void Checkbox::Show() {
    Window::Show();
    this->Redraw();
}

void Checkbox::SetText(const std::string& value) {
    if (value != this->buffer) {
        this->buffer = value;
        this->Redraw();
    }
}

void Checkbox::SetChecked(bool checked) {
    if (checked != this->checked) {
        this->checked = checked;
        this->Redraw();
        this->CheckChanged(this, checked);
    }
}

void Checkbox::Focus() {
    this->focused = true;
    this->Redraw();
}

void Checkbox::Blur() {
    this->focused = false;
    this->Redraw();
}

void Checkbox::Redraw() {
    WINDOW* c = this->GetContent();
    werase(c);

    int len = (int) u8len(this->buffer);
    int cx = this->GetContentWidth();

    std::string symbol = (this->checked ? CHECKED : UNCHECKED);
    std::string ellipsized = symbol + " " + this->buffer;
    text::Ellipsize(ellipsized, cx);

    int64 attrs = this->focused ? CURSESPP_TEXT_FOCUSED : -1LL;

    if (attrs != -1) {
        wattron(c, COLOR_PAIR(attrs));
    }

    wprintw(c, ellipsized.c_str());

    if (attrs != -1) {
        wattroff(c, COLOR_PAIR(attrs));
    }

    this->Repaint();
}

bool Checkbox::KeyPress(const std::string& key) {
    if (key == " " || key == "^M") {
        this->SetChecked(!this->checked);
        return true;
    }
    return false;
}
