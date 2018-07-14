//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "stdafx.h"

#include "ShortcutsWindow.h"
#include "Colors.h"
#include "Text.h"

using namespace cursespp;

ShortcutsWindow::ShortcutsWindow()
: Window(nullptr)
, alignment(text::AlignCenter) {
    this->SetFrameVisible(false);
    this->SetFocusedContentColor(CURSESPP_SHORTCUT_ROW_FOCUSED);
    this->SetContentColor(CURSESPP_SHORTCUT_ROW_NORMAL);
}

ShortcutsWindow::~ShortcutsWindow() {
}

void ShortcutsWindow::SetAlignment(text::TextAlign align) {
    this->alignment = align;
    this->Redraw();
}

void ShortcutsWindow::AddShortcut(
    const std::string& key,
    const std::string& description,
    int64_t attrs)
{
    this->entries.push_back(
        std::shared_ptr<Entry>(new Entry(key, description, attrs)));

    this->Redraw();
}

void ShortcutsWindow::RemoveAll() {
    this->entries.clear();
    this->activeKey = this->originalKey = "";
    this->Redraw();
}

void ShortcutsWindow::SetActive(const std::string& key) {
    this->activeKey = key;
    this->originalKey = "";
    this->Redraw();
}

size_t ShortcutsWindow::CalculateLeftPadding() {
    if (this->alignment == text::AlignLeft) {
        return 0;
    }

    int padding = this->GetContentWidth();

    for (size_t i = 0; i < this->entries.size(); i++) {
        auto e = this->entries[i];
        padding -= u8cols(e->key) + u8cols(e->description) + 5;
    }

    if (padding < 0) {
        return 0;
    }

    if (this->alignment == text::AlignRight) {
        return padding;
    }

    return (padding / 2); /* text::AlignCenter */
}

void ShortcutsWindow::SetChangedCallback(ChangedCallback callback) {
    this->changedCallback = callback;
}

bool ShortcutsWindow::KeyPress(const std::string& key) {
    if (this->changedCallback && this->IsFocused()) {
        int count = (int) this->entries.size();
        if (count > 0) {
            auto& keys = NavigationKeys();

            if (keys.Right(key)) {
                int active = getActiveIndex();
                if (active >= 0 && active + 1 < count) {
                    this->activeKey = this->entries[active + 1]->key;
                }
                else {
                    this->activeKey = this->entries[0]->key;
                }
                this->Redraw();
                return true;
            }
            else if (keys.Left(key)) {
                int active = getActiveIndex();
                if (active > 0) {
                    this->activeKey = this->entries[active - 1]->key;
                }
                else {
                    this->activeKey = this->entries[count - 1]->key;
                }
                this->Redraw();
                return true;
            }
            else if (key == "KEY_ENTER") {
                /* replace the original key we cached when we were forcused originally
                to "commit" the operation, as it'll be swapped back when we lose focus */
                this->originalKey = this->activeKey;

                if (this->changedCallback) {
                    this->changedCallback(this->activeKey);
                }
            }
        }
    }
    return false;
}

bool ShortcutsWindow::MouseEvent(const IMouseHandler::Event& mouseEvent) {
    if (mouseEvent.Button1Clicked()) {
        for (auto entry : this->entries) {
            auto& pos = entry->position;
            if (mouseEvent.x >= pos.offset &&
                mouseEvent.x < pos.offset + pos.width)
            {
                this->activeKey = entry->key;

                this->Redraw();

                if (this->changedCallback) {
                    this->changedCallback(this->activeKey);
                }

                return true;
            }
        }
    }
    return false;
}

void ShortcutsWindow::OnFocusChanged(bool focused) {
    if (focused) {
        this->originalKey = this->activeKey;
    }
    else {
        if (this->originalKey.size()) {
            this->activeKey = this->originalKey;
            this->originalKey = "";
            this->Redraw();
        }
    }
}

int ShortcutsWindow::getActiveIndex() {
    for (int i = 0; i < (int) this->entries.size(); i++) {
        if (this->entries[i]->key == this->activeKey) {
            return i;
        }
    }
    return -1;
}

void ShortcutsWindow::OnRedraw() {
    this->Clear();

    int64_t normalAttrs = COLOR_PAIR(CURSESPP_BUTTON_NORMAL);
    int64_t activeAttrs = COLOR_PAIR(CURSESPP_BUTTON_HIGHLIGHTED);

    WINDOW* c = this->GetContent();

    size_t leftPadding = this->CalculateLeftPadding();
    wmove(c, 0, leftPadding);

    size_t currentX = leftPadding;
    size_t remaining = this->GetContentWidth();
    for (size_t i = 0; i < this->entries.size() && remaining > 0; i++) {
        auto e = this->entries[i];

        int64_t keyAttrs = (e->attrs == -1) ? normalAttrs : COLOR_PAIR(e->attrs);
        keyAttrs = (e->key == this->activeKey) ? activeAttrs : keyAttrs;

        checked_wprintw(c, " ");
        --remaining;

        if (remaining == 0) {
            continue;
        }

        std::string key = " " + e->key + " ";
        std::string value = " " + e->description + " ";

        /* calculate the offset and width, this is used for mouse
        click handling! */
        size_t width = u8cols(key + value);
        e->position.offset = currentX;
        e->position.width = width;
        currentX += 1 + width; /* 1 is the extra leading space */

        /* draw the shortcut key */
        size_t len = u8cols(key);
        if (len > remaining) {
            key = text::Ellipsize(key, remaining);
            len = remaining;
        }

        wattron(c, keyAttrs);
        checked_wprintw(c, key.c_str());
        wattroff(c, keyAttrs);

        remaining -= len;

        if (remaining == 0) {
            continue;
        }

        /* draw the description */
        len = u8cols(value);
        if (len > remaining) {
            value = text::Ellipsize(value, remaining);
            len = remaining;
        }

        checked_wprintw(c, value.c_str());
        remaining -= len;
    }
}
