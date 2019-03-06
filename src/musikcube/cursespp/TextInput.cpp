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
#include <limits.h>

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/TextInput.h>

using namespace cursespp;

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
        if (offset != std::string::npos) {
            size_t end = u8offset(value, position);
            value.erase(offset, end - offset);
            return true;
        }
    }

    return false;
}

TextInput::TextInput(TextInput::Style style, IInput::InputMode inputMode)
: Window()
, bufferLength(0)
, position(0)
, style(style)
, inputMode(inputMode)
, enterEnabled(true)
, truncate(false) {
    if (style == StyleLine) {
        this->SetFrameVisible(false);
    }
}

TextInput::TextInput(IInput::InputMode inputMode)
: TextInput(TextInput::StyleBox, inputMode) {
}

TextInput::~TextInput() {
}

void TextInput::OnRedraw() {
    WINDOW* c = this->GetContent();
    werase(c);

    std::string trimmed;
    int contentWidth = GetContentWidth();
    int columns = u8cols(buffer);

    /* if the string is larger than our width, we gotta trim it for
    display purposes... */
    if (position > contentWidth) {
        trimmed = u8substr(this->buffer, position - contentWidth, INT_MAX);
    }
    else {
        trimmed = buffer;
    }

    if (!columns && hintText.size()) {
        /* draw the hint if we have one and there's no string yet */
        int64_t color = Color(Color::TextDisabled);
        wattron(c, color);
        wmove(c, 0, 0);
        checked_waddstr(c, u8substr(hintText, 0, contentWidth).c_str());
        wattroff(c, color);
    }
    else {
        /* mask the string if we're in password mode */
        if (inputMode == InputPassword) {
            trimmed = std::string(columns, '*');
        }

        /* if we're in "Line" mode and the string is short, pad the
        end with a bunch of underscores */
        if (style == StyleLine) {
            int remaining = contentWidth - columns;
            if (remaining > 0) {
                trimmed += std::string(remaining, '_');
            }
        }

        /* finally, draw the offset/trimmed, potentially masked, padded
        string to the output */
        checked_waddstr(c, trimmed.c_str());
    }
}

size_t TextInput::Length() {
    return this->bufferLength;
}

size_t TextInput::Position() {
    /* note we return the COLUMN offset, not the physical or logical
    character offset! */
    return u8cols(u8substr(this->buffer, 0, this->position));
}

bool TextInput::Write(const std::string& key) {
    /* one character at a time. if it's more than one character, we're
    dealing with an escape sequence and should not print it unless
    the input mode allows for modifiers */
    int len = u8len(key);
    if (len == 1 || (len > 1 && this->inputMode == InputRaw)) {
        if (this->inputMode == InputRaw) {
            auto& bl = this->rawBlacklist;
            if (std::find(bl.begin(), bl.end(), key) != bl.end()) {
                return false;
            }
            this->buffer = key;
            this->bufferLength = len;
            this->position = len;
        }
        else {
            if (truncate) {
                int cols = u8cols(this->buffer);
                if (cols >= this->GetWidth()) {
                    return false;
                }
            }

            size_t offset = u8offset(this->buffer, this->position);
            offset = (offset == std::string::npos) ? 0 : offset;
            this->buffer.insert(offset, key);
            this->bufferLength = u8len(buffer);
            this->position += len;
        }

        this->TextChanged(this, this->buffer);
        this->Redraw();
        return true;
    }

    return false;
}

void TextInput::SetTruncate(bool truncate) {
    if (this->truncate != truncate) {
        this->truncate = truncate;
    }
}

void TextInput::SetEnterEnabled(bool enabled) {
    this->enterEnabled = enabled;
}

void TextInput::SetRawKeyBlacklist(const std::vector<std::string>&& blacklist) {
    this->rawBlacklist = blacklist;
}

bool TextInput::KeyPress(const std::string& key) {
    if (this->inputMode == InputMode::InputRaw) {
        return false;
    }

    if (key == "M-KEY_BACKSPACE") {
        this->SetText("");
        return true;
    }
    else if (key == "KEY_BACKSPACE") {
        if (this->position > 0) {
            if (removeUtf8Char(this->buffer, this->position)) {
                --this->bufferLength;
                this->Redraw();
                this->position = std::max(0, this->position - 1);
                this->TextChanged(this, this->buffer);
            }
        }
        return true;
    }
    else if (key == "KEY_ENTER") {
        if (enterEnabled) {
            this->EnterPressed(this);
            return true;
        }
        else {
            return false;
        }
    }
    else if (key == "KEY_LEFT") {
        return this->OffsetPosition(-1);
    }
    else if (key == "KEY_RIGHT") {
        return this->OffsetPosition(1);
    }
    else if (key == "KEY_HOME") {
        this->position = 0;
        this->Redraw();
        return true;
    }
    else if (key == "KEY_END") {
        this->position = this->bufferLength;
        this->Redraw();
        return true;
    }
    else if (key == "KEY_DC") {
        if ((int) this->bufferLength > this->position) {
            removeUtf8Char(this->buffer, this->position + 1);
            this->bufferLength = u8len(buffer);
            this->Redraw();
            this->TextChanged(this, this->buffer);
            return true;
        }
    }

    return false;
}

bool TextInput::OffsetPosition(int delta) {
    int actual = this->position + delta;
    actual = std::max(0, std::min((int) this->bufferLength, actual));

    if (this->position != actual) {
        this->position = actual;
        this->Redraw();
        return true; /* moved */
    }

    return false; /* didn't move */
}

void TextInput::SetText(const std::string& value) {
    if (value != this->buffer) {
        this->buffer = value;
        this->bufferLength = u8len(buffer);
        this->position = this->bufferLength;
        this->TextChanged(this, this->buffer);
        this->Redraw();
    }
}

void TextInput::SetHint(const std::string& hint) {
    this->hintText = hint;
    this->Redraw();
}

bool TextInput::MouseEvent(const IMouseHandler::Event& event) {
    if (event.Button1Clicked()) {
        this->position = std::max(0, std::min((int)this->bufferLength, event.x));
        this->FocusInParent();
        return true;
    }
    return false;
}