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

#include "stdafx.h"
#include "ShortcutsWindow.h"

#include <cursespp/Colors.h>
#include <cursespp/Text.h>

using namespace cursespp;
using namespace musik::box;

ShortcutsWindow::ShortcutsWindow()
: Window(nullptr) {
    this->SetFrameVisible(false);
}

ShortcutsWindow::~ShortcutsWindow() {
}

void ShortcutsWindow::AddShortcut(
    const std::string& key,
    const std::string& description)
{
    this->entries.push_back(
        std::shared_ptr<Entry>(new Entry(key, description)));
}

void ShortcutsWindow::Show() {
    Window::Show();
    this->Redraw();
}

void ShortcutsWindow::Redraw() {
    std::string value;

    int64 active = COLOR_PAIR(CURSESPP_HIGHLIGHTED_SELECTED_LIST_ITEM);

    WINDOW* c = this->GetContent();
    werase(c);
    wbkgd(c, COLOR_PAIR(CURSESPP_SELECTED_LIST_ITEM));

    for (size_t i = 0; i < this->entries.size(); i++) {
        auto e = this->entries[i];

        wprintw(c, " ");

        wattron(c, active);
        wprintw(c, " %s ", e->key.c_str());
        wattroff(c, active);

        wprintw(c, " %s ", e->description.c_str());

        //if (i != this->entries.size() - 1) {
        //    //wattron(c, separator);
        //    wprintw(c, " ▪ ");
        //    //wattroff(c, separator);
        //}
    }
}