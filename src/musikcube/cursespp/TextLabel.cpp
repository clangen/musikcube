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

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/Text.h>
#include <cursespp/ILayout.h>

#include <cursespp/TextLabel.h>

using namespace cursespp;

TextLabel::TextLabel()
: Window()
, alignment(text::AlignLeft) {
    this->ApplyDefaultStyle();
}

TextLabel::TextLabel(const std::string& value)
: Window()
, alignment(text::AlignLeft) {
    this->ApplyDefaultStyle();
    this->SetText(value);
}

TextLabel::TextLabel(const std::string& value, const text::TextAlign alignment)
: Window()
, alignment(alignment) {
    this->ApplyDefaultStyle();
    this->SetText(value, alignment);
}

TextLabel::~TextLabel() {
}

void TextLabel::ApplyDefaultStyle() {
    this->SetFrameVisible(false);
    this->SetContentColor(Color::Default);
    this->SetFocusedContentColor(Color::TextFocused);
    this->bold = false;
}

void TextLabel::OnRedraw() {
    std::string aligned = text::Align(
        this->buffer, alignment, this->GetContentWidth());

    WINDOW* c = this->GetContent();

    Color color = this->IsFocused()
        ? this->GetFocusedContentColor()
        : this->GetContentColor();

    wattron(c, color);
    if (this->bold) { wattron(c, A_BOLD); }
    wmove(c, 0, 0);
    checked_waddstr(c, aligned.c_str());
    if (this->bold) { wattroff(c, A_BOLD); }
    wattroff(c, color);
}

void TextLabel::SetText(const std::string& value, const text::TextAlign alignment) {
    if (value != this->buffer || alignment != this->alignment) {
        this->buffer = value;
        this->alignment = alignment;
        this->Redraw();
    }
}

void TextLabel::SetAlignment(const text::TextAlign alignment) {
    if (this->alignment != alignment) {
        this->alignment = alignment;
        this->Redraw();
    }
}

void TextLabel::SetText(const std::string& value) {
    this->SetText(value, this->alignment);
}

void TextLabel::SetBold(bool bold) {
    if (bold != this->bold) {
        this->bold = bold;
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

bool TextLabel::MouseEvent(const IMouseHandler::Event& event) {
    if (event.Button1Clicked()) {
        this->FocusInParent();
        this->Activated(this);
        return true;
    }
    return false;
}
