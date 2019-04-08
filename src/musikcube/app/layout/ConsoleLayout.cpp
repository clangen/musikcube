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
#include <app/layout/ConsoleLayout.h>
#include <core/i18n/Locale.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/ToastOverlay.h>
#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/version.h>

using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace cursespp;

ConsoleLayout::ConsoleLayout(ConsoleLogger* logger)
: LayoutBase()
, logger(logger) {
    this->adapter = logger->Adapter();
    this->adapter->Changed.connect(this, &ConsoleLayout::OnAdapterChanged);
    this->listView = std::make_shared<ListWindow>(this->adapter);
    this->listView->SelectionChanged.connect(this, &ConsoleLayout::OnSelectionChanged);
    this->listView->EntryActivated.connect(this, &ConsoleLayout::OnItemActivated);
    this->listView->SetFrameTitle(_TSTR("console_list_title"));
    this->AddWindow(this->listView);
    this->listView->SetFocusOrder(0);
    this->listView->ScrollToBottom();
}

void ConsoleLayout::OnLayout() {
    LayoutBase::OnLayout();
    int cx = this->GetContentWidth();
    int cy = this->GetContentHeight();
    this->listView->MoveAndResize(0, 0, cx, cy);
}

void ConsoleLayout::OnAdapterChanged(SimpleScrollAdapter* adapter) {
    if (this->scrolledToBottom) {
        this->listView->ScrollToBottom();
    }
}

void ConsoleLayout::OnSelectionChanged(cursespp::ListWindow* window, size_t index, size_t prev) {
    auto count = this->logger->Adapter()->GetEntryCount();
    this->scrolledToBottom = index == (count - 1);
}

void ConsoleLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
    if (visible && this->scrolledToBottom) {
        this->listView->ScrollToBottom();
    }
}

void ConsoleLayout::OnItemActivated(cursespp::ListWindow* window, size_t index) {
    ToastOverlay::Show(this->logger->Adapter()->StringAt(index), -1);
}

bool ConsoleLayout::KeyPress(const std::string& kn) {
    if (kn == "x") {
        this->adapter->Clear();
        return true;
    }
    return LayoutBase::KeyPress(kn);
}

void ConsoleLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), _TSTR("shortcuts_console"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
                this->Broadcast(message::JumpToSettings);
            }
            if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
                this->Broadcast(message::JumpToLibrary);
            }
            else if (key == App::Instance().GetQuitKey()) {
                App::Instance().Quit();
            }
            else {
                this->KeyPress(key);
            }
        });

        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateConsole));
    }
}
