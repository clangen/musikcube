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

#include "LocalLibrarySettingsLayout.h"

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/SingleLineEntry.h>

#include <core/library/Indexer.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/library/LibraryFactory.h>
#include <core/support/PreferenceKeys.h>
#include <core/support/Messages.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>

using namespace std::placeholders;
using namespace musik;
using namespace musik::core;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

using EntryPtr = IScrollAdapter::EntryPtr;
static bool showDotfiles = false;

LocalLibrarySettingsLayout::LocalLibrarySettingsLayout()
: LayoutBase()
, library(LibraryFactory::Instance().DefaultLocalLibrary())
, indexer(library->Indexer()) {
    this->browseAdapter.reset(new DirectoryAdapter());
    this->addedPathsAdapter.reset(new SimpleScrollAdapter());
    this->SetFocusMode(FocusModeTerminating);
    this->InitializeWindows();
}

LocalLibrarySettingsLayout::~LocalLibrarySettingsLayout() {
}

void LocalLibrarySettingsLayout::OnLayout() {
    int cx = this->GetWidth();
    int startY = 0;
    int leftX = 0;
    int leftWidth = cx / 3; /* 1/3 width */
    int rightX = leftWidth;
    int rightWidth = cx - rightX; /* remainder (~2/3) */
    int pathsHeight = this->GetHeight();
    this->browseList->MoveAndResize(leftX, 0, leftWidth, pathsHeight);
    this->addedPathsList->MoveAndResize(rightX, 0, rightWidth, pathsHeight);
}

void LocalLibrarySettingsLayout::LoadPreferences() {
    this->addedPathsAdapter->Clear();

    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);

    for (size_t i = 0; i < paths.size(); i++) {
        auto v = paths.at(i);
        auto e = EntryPtr(new SingleLineEntry(v));
        this->addedPathsAdapter->AddEntry(e);
    }

    this->addedPathsList->OnAdapterChanged();
}

Color LocalLibrarySettingsLayout::ListItemDecorator(
    ScrollableWindow* scrollable,
    size_t index,
    size_t line,
    IScrollAdapter::EntryPtr entry)
{
    if (scrollable == this->addedPathsList.get() ||
        scrollable == this->browseList.get())
    {
         ListWindow* lw = static_cast<ListWindow*>(scrollable);
         if (lw->GetSelectedIndex() == index) {
             return Color::ListItemHighlighted;
         }
    }
    return Color::Default;
}

void LocalLibrarySettingsLayout::InitializeWindows() {
    this->SetFrameVisible(false);

    this->addedPathsList.reset(new cursespp::ListWindow(this->addedPathsAdapter));
    this->addedPathsList->SetFrameTitle(_TSTR("settings_backspace_to_remove"));

    this->browseList.reset(new cursespp::ListWindow(this->browseAdapter));
    this->browseList->SetFrameTitle(_TSTR("settings_space_to_add"));

    ScrollAdapterBase::ItemDecorator decorator =
        std::bind(
            &LocalLibrarySettingsLayout::ListItemDecorator,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4);

    this->addedPathsAdapter->SetItemDecorator(decorator);
    this->browseAdapter->SetItemDecorator(decorator);

    int order = 0;
    this->browseList->SetFocusOrder(order++);
    this->addedPathsList->SetFocusOrder(order++);

    this->AddWindow(this->browseList);
    this->AddWindow(this->addedPathsList);
}

void LocalLibrarySettingsLayout::ToggleShowDotFiles() {
    showDotfiles = !showDotfiles;
    this->browseAdapter->SetDotfilesVisible(showDotfiles);
    this->browseList->OnAdapterChanged();
}

void LocalLibrarySettingsLayout::AddSelectedDirectory() {
    size_t index = this->browseList->GetSelectedIndex();

    if (index == ListWindow::NO_SELECTION) {
        index = 0;
    }

    std::string path = this->browseAdapter->GetFullPathAt(index);

    if (path.size()) {
        this->indexer->AddPath(path);
        this->LoadPreferences();
        this->library->Indexer()->Schedule(IIndexer::SyncType::Local);
    }
}

void LocalLibrarySettingsLayout::RemoveSelectedDirectory() {
    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);

    size_t index = this->addedPathsList->GetSelectedIndex();
    if (index != ListWindow::NO_SELECTION) {
        this->indexer->RemovePath(paths.at(index));
        this->LoadPreferences();
        this->library->Indexer()->Schedule(IIndexer::SyncType::Local);
    }
}

void LocalLibrarySettingsLayout::DrillIntoSelectedDirectory() {
    size_t selectIndexAt = this->browseAdapter->Select(this->browseList.get());

    if (selectIndexAt == DirectoryAdapter::NO_INDEX) {
        selectIndexAt = 0;
    }

    this->browseList->SetSelectedIndex(selectIndexAt);
    this->browseList->ScrollTo(selectIndexAt);
}

bool LocalLibrarySettingsLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        if (this->GetFocus() == this->browseList) {
            this->DrillIntoSelectedDirectory();
            return true;
        }
    }
    else if (key == " ") {
        if (this->GetFocus() == this->browseList) {
            this->AddSelectedDirectory();
            return true;
        }
    }
    else if (key == "KEY_BACKSPACE" || key == "KEY_DC") { /* backspace */
        if (this->GetFocus() == this->addedPathsList) {
            this->RemoveSelectedDirectory();
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
