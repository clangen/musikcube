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

#include "TextInput.h"

#include <boost/algorithm/string.hpp>

using namespace cursespp;

inline static void redrawContents(IWindow &window, const std::string& text) {
    WINDOW* c = window.GetContent();
    werase(c);
    wprintw(c, text.c_str());
    window.Repaint();
}

inline static void removeUtf8Char(std::string& value) {
    std::string::iterator it = value.end();
    std::string::iterator start = value.begin();
    if (it != start) {
        utf8::prior(it, start);
        value = std::string(value.begin(), it);
    }
}

TextInput::TextInput()
: Window() {
}

TextInput::~TextInput() {
}

void TextInput::Show() {
    Window::Show();
    redrawContents(*this, buffer);
}

void TextInput::Write(const std::string& key) {
    if (key == "M-bksp") {
        this->buffer = "";
        redrawContents(*this, "");
        this->TextChanged(this, "");
    }
    else if (key == "^H" || key == "^?" || key == "KEY_BACKSPACE") { /* backspace */
        removeUtf8Char(this->buffer);
        this->TextChanged(this, this->buffer);
    }
    else if (key == "^M") { /* return */
        this->EnterPressed(this);
    }
    else {
        /* one character at a time. if it's more than one character, we're
        dealing with an escape sequence and should not print it. */
        if (u8len(key) == 1) {
            this->buffer += key;
            this->TextChanged(this, this->buffer);
        }
    }

    this->bufferLength = u8len(buffer);
    redrawContents(*this, buffer);
}

void TextInput::SetText(const std::string& value) {
    if (value != this->buffer) {
        this->buffer = value;
        this->bufferLength = u8len(buffer);
        this->TextChanged(this, this->buffer);
        redrawContents(*this, buffer);
    }
}