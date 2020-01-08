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
#include <cursespp/AppLayout.h>
#include <cursespp/Screen.h>
#include <cursespp/Colors.h>

#include <map>

using namespace cursespp;

static std::map<ILayout*, int> lastFocusMap;

#define ENABLE_DEMO_MODE 0

#if ENABLE_DEMO_MODE
static std::string lastKey;
static int lastKeyRepeat = 0;
#endif

static int last(ILayout* layout) {
    auto it = lastFocusMap.find(layout);
    return (it == lastFocusMap.end()) ? 0 : it->second;
}

static void last(ILayout* layout, int last) {
    lastFocusMap[layout] = last;
}

AppLayout::AppLayout(cursespp::App& app)
: shortcutsFocused(false)
, topLevelLayout(nullptr)
, LayoutBase() {
    this->Initialize();
    this->EnableDemoModeIfNecessary();
}

AppLayout::~AppLayout() {
}

void AppLayout::OnLayout() {
    size_t cx = Screen::GetWidth() - (paddingL + paddingR);
    size_t cy = Screen::GetHeight() - (paddingT + paddingB);

#if ENABLE_DEMO_MODE
    this->hotkey->MoveAndResize(0, cy - 1, cx, 1);
    --cy;
#endif

    bool sf = this->shortcutsFocused;
    int mainCyOffset = this->autoHideCommandBar ? 0 : 1;

    if (this->layout) {
        this->layout->MoveAndResize(paddingL, paddingT, cx, cy - mainCyOffset);
        this->layout->Show();
        this->layout->BringToTop();
    }

    this->shortcuts->MoveAndResize(
        0, Screen::GetHeight() - 1, Screen::GetWidth(), 1);

    if (this->autoHideCommandBar) {
        if (sf) {
            this->shortcuts->Show();
            this->shortcuts->BringToTop();
        }
        else {
            this->shortcuts->Hide();
        }
    }
    else {
        this->shortcuts->Show();
    }
}

void AppLayout::Initialize() {
    this->shortcuts.reset(new ShortcutsWindow());
    this->AddWindow(this->shortcuts);

#if ENABLE_DEMO_MODE
    this->hotkey.reset(new TextLabel());
    this->hotkey->SetContentColor(Color::Footer);
    this->hotkey->SetText("keypress: <none>", text::AlignCenter);
    this->AddWindow(this->hotkey);
#endif
}

void AppLayout::SetPadding(size_t t, size_t l, size_t b, size_t r) {
    this->paddingT = t;
    this->paddingL = l;
    this->paddingB = b;
    this->paddingR = r;
}

cursespp::IWindowPtr AppLayout::GetFocus() {
    if (this->shortcutsFocused) {
        return this->shortcuts;
    }
    if (this->layout) {
        return this->layout->GetFocus();
    }
    return cursespp::IWindowPtr();
}

IWindowPtr AppLayout::FocusNext() {
    if (this->shortcutsFocused) {
        return this->BlurShortcuts();
    }
    else if (this->layout) {
        return this->layout->FocusNext();
    }
    return cursespp::IWindowPtr();
}

IWindowPtr AppLayout::FocusPrev() {
    if (this->shortcutsFocused) {
        return this->BlurShortcuts();
    }
    else if (this->layout) {
        return this->layout->FocusPrev();
    }
    return cursespp::IWindowPtr();
}

void AppLayout::SetLayout(std::shared_ptr<cursespp::LayoutBase> layout) {
    if (layout != this->layout) {
        if (this->layout) {
            this->RemoveWindow(this->layout);
            last(this->layout.get(), this->layout->GetFocusIndex());
            this->layout->Hide();
        }

        this->lastFocus.reset();

        if (this->topLevelLayout) {
            this->topLevelLayout->SetShortcutsWindow(nullptr);
        }

        this->layout = layout;
        this->shortcuts->RemoveAll();

        if (this->layout) {
            this->topLevelLayout = dynamic_cast<ITopLevelLayout*>(layout.get());
            if (this->topLevelLayout) {
                this->topLevelLayout->SetShortcutsWindow(this->shortcuts.get());
            }

            this->AddWindow(this->layout);
            this->layout->SetFocusOrder(0);
            this->Layout();

            if (!this->shortcuts->IsFocused()) {
                auto lastFocusIndex = last(this->layout.get());
                this->layout->SetFocusIndex(lastFocusIndex);
            }
        }
    }
}

cursespp::IWindowPtr AppLayout::BlurShortcuts() {
    this->shortcutsFocused = false;
    this->shortcuts->Hide();
    this->shortcuts->Blur();

    if (this->layout) {
        bool refocused = false;
        if (this->lastFocus) {
            refocused = this->layout->SetFocus(this->lastFocus);
            this->lastFocus.reset();
        }

        if (!refocused) {
            this->layout->FocusFirst();
        }
    }

    this->Layout();
    return this->layout ? this->layout->GetFocus() : IWindowPtr();
}

void AppLayout::FocusShortcuts() {
    this->shortcuts->Show();
    this->Layout();
    this->shortcuts->Focus();

    if (this->layout) {
        this->lastFocus = this->layout->GetFocus();
        this->layout->SetFocus(IWindowPtr());
    }

    this->shortcuts->Focus();
}

bool AppLayout::KeyPress(const std::string& key) {
    /* otherwise, see if the user is monkeying around with the
    shortcut bar focus... */
    if (key == "^["  ||
        (key == "KEY_ENTER" && this->shortcutsFocused) ||
        (key == "KEY_UP" && this->shortcutsFocused))
    {
        this->shortcutsFocused = !this->shortcutsFocused;
        if (this->shortcutsFocused) {
            this->FocusShortcuts();
        }
        else {
            this->BlurShortcuts();
        }
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

    /* otherwise, pass along to our child layout */
    return this->layout ? this->layout->KeyPress(key) : false;
}

void AppLayout::SetAutoHideCommandBar(bool autoHide) {
    if (autoHide != this->autoHideCommandBar) {
        this->autoHideCommandBar = autoHide;
        this->Layout();
    }
}

bool AppLayout::GetAutoHideCommandBar() {
    return this->autoHideCommandBar;
}

void AppLayout::EnableDemoModeIfNecessary() {
#if ENABLE_DEMO_MODE
    App::Instance().SetKeyHook([this](const std::string& key) -> bool {
        static std::map<std::string, std::string> SANITIZE = {
            { "^I", "TAB" }, { " ", "SPACE" }, { "^[", "ESC" }
        };

        auto it = SANITIZE.find(key);
        std::string normalized = (it == SANITIZE.end()) ? key : it->second;

        if (normalized == lastKey) {
            ++lastKeyRepeat;
            if (lastKeyRepeat >= 2) {
                normalized = normalized + " (x" + std::to_string(lastKeyRepeat) + ")";
            }
        }
        else {
            lastKey = normalized;
            lastKeyRepeat = 1;
        }

        std::string keypress = "keypress: " + (normalized.size() ? normalized : "<none>");
        this->hotkey->SetText(keypress, text::AlignCenter);
        return false;
    });
#endif
}