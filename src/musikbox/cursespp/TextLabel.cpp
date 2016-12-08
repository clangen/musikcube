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

#include "TextLabel.h"

using namespace cursespp;

TextLabel::TextLabel()
: Window()
, alignment(text::AlignLeft) {
    this->SetFrameVisible(false);
}

TextLabel::~TextLabel() {
}

void TextLabel::OnRedraw() {
    std::string aligned = text::Align(
        this->buffer, alignment, this->GetContentWidth());

    WINDOW* c = this->GetContent();

    int64 attrs = this->GetContentColor();
    if (attrs != -1) {
        wbkgd(c, COLOR_PAIR(attrs));
    }
    else {
        werase(c);
    }

    attrs = this->IsFocused() ? CURSESPP_TEXT_FOCUSED : -1LL;

    if (attrs != -1) {
        wattron(c, COLOR_PAIR(attrs));
    }

    wmove(c, 0, 0);
    waddstr(c, aligned.c_str());

    if (attrs != -1) {
        wattroff(c, COLOR_PAIR(attrs));
    }
}

void TextLabel::SetText(const std::string& value, const text::TextAlign alignment) {
    if (value != this->buffer || alignment != this->alignment) {
        this->buffer = value;
        this->alignment = alignment;
        this->Redraw();
    }
}

bool TextLabel::KeyPress(const std::string& key) {
    if (this->IsFocused()) {
        if (key == " " || key == "KEY_ENTER") {
            this->Activated(this);
            return true;
        }
    }

    return false;
}