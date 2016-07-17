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

#include <cursespp/Screen.h>

#include "SettingsLayout.h"
#include "MainLayout.h"

using namespace musik::box;
using namespace musik::core;
using namespace cursespp;

MainLayout::MainLayout()
: shortcutsFocused(false)
, topLevelLayout(nullptr)
, LayoutBase() {
    this->Initialize();
    this->prefs = Preferences::ForComponent("settings");
}

MainLayout::~MainLayout() {
}

void MainLayout::Layout() {
    size_t cx = Screen::GetWidth(), cy = Screen::GetHeight();
    this->MoveAndResize(0, 0, cx, cy);

    if (this->layout) {
        this->layout->MoveAndResize(0, 0, cx, cy - 1);
        this->layout->Layout();

        if (this->shortcutsFocused) {
            this->layout->SetFocus(IWindowPtr());
        }
    }

    this->shortcuts->MoveAndResize(0, cy - 1, cx, 1);
}

void MainLayout::Initialize() {
    this->shortcuts.reset(new ShortcutsWindow());
    this->AddWindow(this->shortcuts);
}

cursespp::IWindowPtr MainLayout::GetFocus() {
    if (this->shortcutsFocused) {
        return this->shortcuts;
    }

    if (this->layout) {
        return this->layout->GetFocus();
    }

    return cursespp::IWindowPtr();
}

IWindowPtr MainLayout::FocusNext() {
    return (this->shortcutsFocused)
        ? this->shortcuts : this->layout->FocusNext();
}
IWindowPtr MainLayout::FocusPrev() {
    return (this->shortcutsFocused)
        ? this->shortcuts : this->layout->FocusPrev();
}

void MainLayout::SetMainLayout(std::shared_ptr<cursespp::LayoutBase> layout) {
    if (layout != this->layout) {
        if (this->layout) {
            if (this->lastFocus) {
                this->layout->SetFocus(this->lastFocus);
            }

            this->RemoveWindow(this->layout);
            this->layout->Hide();
            this->lastFocus.reset();
        }

        if (this->topLevelLayout) {
            this->topLevelLayout->SetShortcutsWindow(nullptr);
        }

        this->layout = layout;
        this->topLevelLayout = dynamic_cast<ITopLevelLayout*>(layout.get());

        this->shortcuts->RemoveAll();
        if (this->topLevelLayout) {
            this->topLevelLayout->SetShortcutsWindow(this->shortcuts.get());
        }

        this->AddWindow(layout);
        this->layout->SetFocusOrder(0);
        this->Show();
        this->Layout();
    }
}

bool MainLayout::KeyPress(const std::string& key) {
    if (key == "^[" && prefs->GetBool(GENERAL_PREFS_FOCUS_SHORTCUTS)) {
        this->shortcutsFocused = !this->shortcutsFocused;

        if (this->shortcutsFocused) {
            this->shortcuts->Focus();

            if (this->layout) {
                this->lastFocus = this->layout->GetFocus();
                this->layout->SetFocus(IWindowPtr());
            }
        }
        else {
            this->shortcuts->Blur();

            if (this->layout) {
                bool refocused = false;
                if (this->lastFocus) {
                    refocused = this->layout->SetFocus(this->lastFocus);
                    this->lastFocus.reset();
                }

                if (!refocused) {
                    this->layout->FocusNext();
                }
            }
        }

        this->shortcutsFocused
            ? this->shortcuts->Focus()
            : this->shortcuts->Blur();

        return true;
    }

    if (this->shortcutsFocused) {
        if (key == "KEY_DOWN" || key == "KEY_LEFT" ||
            key == "KEY_UP" || key == "KEY_RIGHT")
        {
            /* layouts allow focusing via TAB and sometimes arrow
            keys. suppress these from bubbling. */
            return true;
        }
    }

    return this->layout ? this->layout->KeyPress(key) : false;
}
