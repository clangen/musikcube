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
#include "HotkeysLayout.h"
#include <core/support/Common.h>
#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/DialogOverlay.h>
#include <app/util/Hotkeys.h>
#include <app/model/HotkeysAdapter.h>
#include <app/overlay/ReassignHotkeyOverlay.h>
#include <app/util/Messages.h>
#include <ctime>
#include <time.h>

using namespace cursespp;
using namespace musik::cube;
using namespace musik::core;

using Entry = IScrollAdapter::EntryPtr;
using Callback = std::function<void()>;

static std::string formattedTime() {
    char buffer[128];
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%F-%H-%M-%S", timeinfo);
    return std::string(buffer);
}

static void confirmResetHotkeys(Callback cb) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("hotkeys_reset_all_title"))
        .SetMessage(_TSTR("hotkeys_reset_all_message"))
        .AddButton("^[", "ESC", _TSTR("button_no"))
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            _TSTR("button_yes"),
            [cb](const std::string& str) {
                Hotkeys::Reset();
                if (cb) { cb(); }
            });

    App::Overlays().Push(dialog);
}

static void checkConflictAndSave(Hotkeys::Id id, const std::string& key, Callback cb) {
    const std::string existing = Hotkeys::Existing(key);

    if (existing == Hotkeys::Name(id)) {
        return;
    }

    if (existing.size()) {
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        std::string message = _TSTR("hotkeys_conflict_message");
        ReplaceAll(message, "{{hotkey}}", key);
        ReplaceAll(message, "{{existing}}", existing);

        (*dialog)
            .SetTitle(_TSTR("hotkeys_conflict_title"))
            .SetMessage(message)
            .AddButton("^[", "ESC", _TSTR("button_no"))
            .AddButton(
                "KEY_ENTER",
                "ENTER",
                _TSTR("button_yes"),
                [id, key, cb](const std::string& str) {
                    Hotkeys::Set(id, key);
                    if (cb) { cb(); }
                });

        App::Overlays().Push(dialog);
    }
    else {
        Hotkeys::Set(id, key);
        if (cb) { cb(); }
    }
}

static void backupAndShowDialog() {
    std::string dir = NormalizeDir(GetDataDirectory());
    std::string in = dir + "hotkeys.json";
    std::string out = dir + "hotkeys-" + formattedTime() + ".json";

    if (CopyFile(in, out)) {
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        std::string message = _TSTR("hotkeys_backup_success_message");
        ReplaceAll(message, "{{path}}", out);

        (*dialog)
            .SetTitle(_TSTR("hotkeys_backup_success_title"))
            .SetMessage(message)
            .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

        App::Overlays().Push(dialog);
    }
    else {
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        std::string message = _TSTR("hotkeys_backup_failure_message");
        ReplaceAll(message, "{{path}}", dir);

        (*dialog)
            .SetTitle(_TSTR("hotkeys_backup_failure_title"))
            .SetMessage(message)
            .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

        App::Overlays().Push(dialog);
    }
}

static void showDeleteOverlay(Hotkeys::Id id, Callback cb) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    std::string message = _TSTR("hotkeys_delete_binding_message");
    ReplaceAll(message, "{{key}}", Hotkeys::Name(id));
    ReplaceAll(message, "{{default}}", Hotkeys::Default(id));

    (*dialog)
        .SetTitle(_TSTR("hotkeys_delete_binding_title"))
        .SetMessage(message)
        .AddButton("^[", "ESC", _TSTR("button_no"))
        .AddButton(
            "KEY_ENTER",
            "ENTER",
            _TSTR("button_yes"),
            [id, cb](const std::string& str) {
                Hotkeys::Set(id, Hotkeys::Default(id));
                if (cb) { cb(); }
            });

    App::Overlays().Push(dialog);
}

HotkeysLayout::HotkeysLayout() {
    auto adapter = std::make_shared<HotkeysAdapter>();

    adapter->SetItemDecorator([this](ScrollableWindow*, size_t index, size_t, Entry) -> int64_t {
        if (this->listWindow->GetSelectedIndex() == index) {
            return COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM);
        }
        return -1LL;
    });

    this->listWindow = std::make_shared<ListWindow>();
    this->listWindow->SetFrameVisible(true);
    this->listWindow->SetAdapter(adapter);
    this->listWindow->SetFrameTitle(_TSTR("hotkeys_title"));
    this->listWindow->EntryActivated.connect(this, &HotkeysLayout::OnEntryActivated);
    this->AddWindow(this->listWindow);
    this->listWindow->SetFocusOrder(0);
}

HotkeysLayout::~HotkeysLayout() {
}

void HotkeysLayout::OnEntryActivated(cursespp::ListWindow* list, size_t index) {
    Hotkeys::Id id = static_cast<Hotkeys::Id>(index);
    auto shortcuts = this->shortcuts;

    ReassignHotkeyOverlay::Show(id, [this, list, id](std::string key) {
        checkConflictAndSave(id, key, [this, list]() {
            list->OnAdapterChanged();
            this->SetShortcutsWindow(this->shortcuts);
        });
    });
}

void HotkeysLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    this->shortcuts = shortcuts;

    if (shortcuts) {
        shortcuts->RemoveAll();

        shortcuts->AddShortcut(
            Hotkeys::Get(Hotkeys::HotkeysResetToDefault),
            _TSTR("hotkeys_reset_defaults"));

        shortcuts->AddShortcut(
            Hotkeys::Get(Hotkeys::HotkeysBackup),
            _TSTR("hotkeys_backup"));

        shortcuts->AddShortcut(
            Hotkeys::Get(Hotkeys::NavigateLibrary),
            _TSTR("shortcuts_library"));

        shortcuts->AddShortcut(
            Hotkeys::Get(Hotkeys::NavigateSettings),
            _TSTR("shortcuts_settings"));

        shortcuts->AddShortcut("^D", _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            this->KeyPress(key);
        });
    }
}

bool HotkeysLayout::KeyPress(const std::string& kn) {
    auto refresh = [this]() {
        this->listWindow->OnAdapterChanged();
        this->SetShortcutsWindow(this->shortcuts);
    };

    if (kn == "KEY_DC") {
        auto index = this->listWindow->GetSelectedIndex();
        if (index != ListWindow::NO_SELECTION) {
            showDeleteOverlay(static_cast<Hotkeys::Id>(index), refresh);
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::HotkeysResetToDefault, kn)) {
        confirmResetHotkeys(refresh);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::HotkeysBackup, kn)) {
        backupAndShowDialog();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateSettings, kn)) {
        this->BroadcastMessage(message::JumpToSettings);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibrary, kn)) {
        this->BroadcastMessage(message::JumpToLibrary);
        return true;
    }

    return LayoutBase::KeyPress(kn);
}

void HotkeysLayout::OnLayout() {
    this->listWindow->MoveAndResize(
        0, 0, this->GetWidth(), this->GetHeight());
}
