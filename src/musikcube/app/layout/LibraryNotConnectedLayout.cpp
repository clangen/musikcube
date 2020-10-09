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
#include <app/layout/LibraryNotConnectedLayout.h>
#include <core/i18n/Locale.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/ToastOverlay.h>
#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>

using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace cursespp;

LibraryNotConnectedLayout::LibraryNotConnectedLayout(ILibraryPtr library)
: LayoutBase()
, library(library) {
    this->messageText = std::make_shared<TextLabel>("library not connected", text::AlignCenter);
    this->AddWindow(this->messageText);
}

void LibraryNotConnectedLayout::OnLayout() {
    LayoutBase::OnLayout();    LayoutBase::OnLayout();
    int cx = this->GetContentWidth();
    int cy = this->GetContentHeight();
    this->messageText->MoveAndResize(0, 0, cx, cy);
    this->messageText->MoveAndResize(1, cy / 2, cx - 2, 1);
}

void LibraryNotConnectedLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
}

bool LibraryNotConnectedLayout::KeyPress(const std::string& kn) {
    return LayoutBase::KeyPress(kn);
}

void LibraryNotConnectedLayout::OnLibraryStateChanged(ILibrary::ConnectionState state) {

}

void LibraryNotConnectedLayout::SetShortcutsWindow(cursespp::ShortcutsWindow* shortcuts) {
    this->shortcuts = shortcuts;

    if (this->shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), _TSTR("shortcuts_console"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
                this->Broadcast(message::JumpToConsole);
            }
            if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
                this->Broadcast(message::JumpToSettings);
            }
            else if (key == App::Instance().GetQuitKey()) {
                App::Instance().Quit();
            }
            this->KeyPress(key);
        });

        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibrary));
    }
}
