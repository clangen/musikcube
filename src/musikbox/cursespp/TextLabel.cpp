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

inline static void redrawContents(
    IWindow &window, 
    const TextLabel::Alignment alignment,
    const std::string& text) 
{
    WINDOW* c = window.GetContent();
    werase(c);

    int len = (int) u8len(text);
    int cx = window.GetContentWidth();

    if (len > cx) {
        std::string ellipsized = text;
        text::Ellipsize(ellipsized, cx);
        wprintw(c, ellipsized.c_str());
    }
    else if (alignment == TextLabel::AlignLeft) {
        wprintw(c, text.c_str());
    }
    else { /* center */
        int leftPad = 
            (alignment == TextLabel::AlignRight)
                ? (cx - len) 
                : (cx - len) / 2;

        std::string padded;
        for (int i = 0; i < leftPad; i++) {
            padded += " ";
        }

        padded += text;
        wprintw(c, padded.c_str());
    }

    window.Repaint();
}

TextLabel::TextLabel()
: Window()
, alignment(AlignLeft) {
    this->SetFrameVisible(false);
}

TextLabel::~TextLabel() {
}

void TextLabel::Show() {
    Window::Show();
    redrawContents(*this, this->alignment, this->buffer);
}

void TextLabel::SetText(const std::string& value, const Alignment alignment) {
    if (value != this->buffer || alignment != this->alignment) {
        this->buffer = value;
        this->alignment = alignment;
        redrawContents(*this, alignment, buffer);
    }
}