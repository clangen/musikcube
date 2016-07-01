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

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/SingleLineEntry.h>

#include <core/library/Indexer.h>
#include <core/library/LocalLibraryConstants.h>
#include <app/query/SearchTrackListQuery.h>

#include "IndexerLayout.h"

using namespace musik::core::library::constants;
using namespace musik::core;
using namespace musik::box;
using namespace cursespp;
using namespace std::placeholders;

#define LABEL_HEIGHT 1

#define TOP(w) w->GetY()
#define BOTTOM(w) (w->GetY() + w->GetHeight())
#define LEFT(w) w->GetX()
#define RIGHT (x) (x->GetX() + w->GetWidth())

typedef IScrollAdapter::EntryPtr EntryPtr;
static bool showDotfiles = false;

IndexerLayout::IndexerLayout(musik::core::LibraryPtr library)
: LayoutBase()
, library(library)
, indexer(library->Indexer()) {
    this->prefs = Preferences::ForComponent(INDEXER_PREFS_COMPONENT);
    this->indexer->PathsUpdated.connect(this, &IndexerLayout::RefreshAddedPaths);
    this->InitializeWindows();
}

IndexerLayout::~IndexerLayout() {
}

void IndexerLayout::OnRemoveMissingCheckChanged(cursespp::Checkbox* cb, bool checked) {
    this->prefs->SetBool(INDEXER_PREFS_REMOVE_MISSING_FILES, checked);
    this->prefs->Save();
}

void IndexerLayout::OnDotfilesCheckChanged(cursespp::Checkbox* cb, bool checked) {
    showDotfiles = !showDotfiles;
    this->browseAdapter.SetDotfilesVisible(showDotfiles);
    this->browseList->OnAdapterChanged();
}

void IndexerLayout::Layout() {
    int x = 0, y = 0;
    int cx = Screen::GetWidth(), cy = Screen::GetHeight();
    this->SetFrameVisible(false);
    this->MoveAndResize(x, y, cx, cy);

    this->title->MoveAndResize(0, 0, cx, LABEL_HEIGHT);

    int startY = BOTTOM(this->title) + 1;
    int leftX = 0;
    int leftWidth = cx / 3; /* 1/3 width */
    int rightX = leftWidth;
    int rightWidth = cx - rightX; /* remainder (~2/3) */

    this->browseLabel->MoveAndResize(leftX + 1, startY, leftWidth - 1, LABEL_HEIGHT);
    this->addedPathsLabel->MoveAndResize(rightX + 1, startY, rightWidth - 1, LABEL_HEIGHT);

    int pathListsY = BOTTOM(this->browseLabel);
    int pathsHeight = (cy - pathListsY) / 2;

    this->browseList->MoveAndResize(leftX, pathListsY, leftWidth, pathsHeight);
    this->addedPathsList->MoveAndResize(rightX, pathListsY, rightWidth, pathsHeight);

    this->dotfileCheckbox->MoveAndResize(1, BOTTOM(this->browseList), cx - 1, LABEL_HEIGHT);
    this->removeCheckbox->MoveAndResize(1, BOTTOM(this->dotfileCheckbox), cx - 1, LABEL_HEIGHT);
}

void IndexerLayout::RefreshAddedPaths() {
    this->addedPathsAdapter.Clear();

    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);

    for (size_t i = 0; i < paths.size(); i++) {
        auto v = paths.at(i);
        auto e = EntryPtr(new SingleLineEntry(v));
        this->addedPathsAdapter.AddEntry(e);
    }

    this->addedPathsList->OnAdapterChanged();
}

int64 IndexerLayout::ListItemDecorator(
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
             return COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM);
         }
    }
    return -1;
}

void IndexerLayout::InitializeWindows() {
    this->title.reset(new TextLabel());
    this->title->SetText("settings", TextLabel::AlignCenter);

    this->browseLabel.reset(new TextLabel());
    this->browseLabel->SetText("browse (SPACE to add)", TextLabel::AlignLeft);

    this->addedPathsLabel.reset(new TextLabel());
    this->addedPathsLabel->SetText("indexed paths (BACKSPACE to remove)", TextLabel::AlignLeft);

    this->addedPathsList.reset(new cursespp::ListWindow(&this->addedPathsAdapter));
    this->browseList.reset(new cursespp::ListWindow(&this->browseAdapter));

    ScrollAdapterBase::ItemDecorator decorator =
        std::bind(
            &IndexerLayout::ListItemDecorator,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4);

    this->addedPathsAdapter.SetItemDecorator(decorator);
    this->browseAdapter.SetItemDecorator(decorator);

    this->dotfileCheckbox.reset(new cursespp::Checkbox());
    this->dotfileCheckbox->SetText("show dotfiles in directory browser");
    this->dotfileCheckbox->CheckChanged.connect(this, &IndexerLayout::OnDotfilesCheckChanged);

    this->removeCheckbox.reset(new cursespp::Checkbox());
    this->removeCheckbox->SetText("remove missing files from library");
    this->removeCheckbox->CheckChanged.connect(this, &IndexerLayout::OnRemoveMissingCheckChanged);

    this->browseList->SetFocusOrder(0);
    this->addedPathsList->SetFocusOrder(1);
    this->dotfileCheckbox->SetFocusOrder(2);
    this->removeCheckbox->SetFocusOrder(3);

    this->AddWindow(this->title);
    this->AddWindow(this->browseLabel);
    this->AddWindow(this->addedPathsLabel);
    this->AddWindow(this->browseList);
    this->AddWindow(this->addedPathsList);
    this->AddWindow(this->dotfileCheckbox);
    this->AddWindow(this->removeCheckbox);

    this->Layout();
}

void IndexerLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->RefreshAddedPaths();
        this->LoadPreferences();
    }
}

void IndexerLayout::LoadPreferences() {
    this->removeCheckbox->SetChecked(this->prefs->GetBool(INDEXER_PREFS_REMOVE_MISSING_FILES, true));
}

void IndexerLayout::AddSelectedDirectory() {
    size_t index = this->browseList->GetSelectedIndex();
    std::string path = this->browseAdapter.GetFullPathAt(index);

    if (path.size()) {
        this->indexer->AddPath(path);
    }
}

void IndexerLayout::RemoveSelectedDirectory() {
    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);
    size_t index = this->addedPathsList->GetSelectedIndex();
    this->indexer->RemovePath(paths.at(index));
}

void IndexerLayout::DrillIntoSelectedDirectory() {
    this->browseAdapter.Select(this->browseList->GetSelectedIndex());
    this->browseList->SetSelectedIndex(0);
    this->browseList->OnAdapterChanged();
}

bool IndexerLayout::KeyPress(const std::string& key) {
    if (key == "^M") {
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
    else if (key == "^H" || key == "^?" || key == "KEY_BACKSPACE") { /* backspace */
        if (this->GetFocus() == this->addedPathsList) {
            this->RemoveSelectedDirectory();
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
