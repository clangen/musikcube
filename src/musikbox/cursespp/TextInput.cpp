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

inline static bool removeUtf8Char(std::string& value, size_t position) {
    /* optimize the normal case, at the end... */
    if (position >= value.size()) {
        std::string::iterator it = value.end();
        std::string::iterator start = value.begin();
        if (it != start) {
            utf8::prior(it, start);
            value = std::string(value.begin(), it);
            return true;
        }
    }
    else {
        size_t offset = u8offset(value, position - 1);
        if (offset >= 0) {
            size_t end = u8offset(value, position);
            value.erase(offset, end - offset);
            return true;
        }
    }

    return false;
}

TextInput::TextInput()
: Window()
, bufferLength(0)
, position(0) {
}

TextInput::~TextInput() {
}

void TextInput::Show() {
    Window::Show();
    redrawContents(*this, buffer);
}

size_t TextInput::Length() {
    return this->bufferLength;
}

size_t TextInput::Position() {
    return this->position;
}

bool TextInput::Write(const std::string& key) {
    if (key == "M-^H" || key == "M-^?" || key == "M-bksp" || key == "M-KEY_BACKSPACE") {
        this->SetText("");
    }
    else if (key == "^H" || key == "^?" || key == "KEY_BACKSPACE") { /* backspace */
        if (this->position > 0) {
            if (removeUtf8Char(this->buffer, this->position)) {
                this->position = std::max(0, this->position - 1);
                this->TextChanged(this, this->buffer);
            }
        }
    }
    else if (key == "^M") { /* return */
        this->EnterPressed(this);
    }
    else {
        /* one character at a time. if it's more than one character, we're
        dealing with an escape sequence and should not print it. */
        if (u8len(key) == 1) {
            this->buffer.insert(u8offset(this->buffer, this->position), key);
            this->TextChanged(this, this->buffer);
            ++this->position;
        }
        else {
            return false;
        }
    }

    this->bufferLength = u8len(buffer);
    redrawContents(*this, buffer);
    return true;
}

bool TextInput::KeyPress(const std::string& key) {
    if (key == "KEY_LEFT") {
        return this->MoveCursor(-1);
    }
    else if (key == "KEY_RIGHT") {
        return this->MoveCursor(1);
    }
    else if (key == "KEY_HOME") {
        this->position = 0;
        redrawContents(*this, buffer);
        return true;
    }
    else if (key == "KEY_END") {
        this->position = this->bufferLength;
        redrawContents(*this, buffer);
        return true;
    }
    else if (key == "KEY_DC") {
        if (this->bufferLength > this->position) {
            removeUtf8Char(this->buffer, this->position + 1);
            this->bufferLength = u8len(buffer);
            redrawContents(*this, buffer);
            return true;
        }
    }

    return false;
}

bool TextInput::MoveCursor(int delta) {
    int actual = this->position + delta;
    actual = std::max(0, std::min((int) this->bufferLength, actual));

    if (this->position != actual) {
        this->position = actual;
        redrawContents(*this, buffer);
        return true; /* moved */
    }

    return false; /* didn't move */
}

void TextInput::SetText(const std::string& value) {
    if (value != this->buffer) {
        this->buffer = value;
        this->bufferLength = u8len(buffer);
        this->position = 0;
        this->TextChanged(this, this->buffer);
        redrawContents(*this, buffer);
    }
}
