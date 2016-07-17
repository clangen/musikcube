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

#include <stdafx.h>
#include <algorithm>
#include "ListWindow.h"

using namespace cursespp;

typedef IScrollAdapter::ScrollPosition ScrollPos;

size_t ListWindow::NO_SELECTION = (size_t) -1;

class EmptyAdapter : public IScrollAdapter {
    public:
        virtual void SetDisplaySize(size_t width, size_t height) { }
        virtual size_t GetEntryCount() { return 0; }
        virtual EntryPtr GetEntry(size_t index) { return IScrollAdapter::EntryPtr(); }
        virtual void DrawPage(ScrollableWindow* window, size_t index, ScrollPosition *result = NULL) { }
};

static EmptyAdapter emptyAdapter;

ListWindow::ListWindow(IScrollAdapter* adapter, IWindow *parent)
: ScrollableWindow(parent)
, adapter(adapter)
, selectedIndex(0) {

}

ListWindow::~ListWindow() {

}

void ListWindow::ScrollToTop() {
    this->SetSelectedIndex(0);
    this->ScrollTo(0);
}

IScrollAdapter& ListWindow::GetScrollAdapter() {
    if (this->adapter) {
        return *this->adapter;
    }

    return emptyAdapter;
}

void ListWindow::ScrollToBottom() {
    IScrollAdapter& adapter = this->GetScrollAdapter();
    this->SetSelectedIndex(std::max((size_t) 0, adapter.GetEntryCount() - 1));
    this->ScrollTo(selectedIndex);
}

void ListWindow::ScrollUp(int delta) {
    IScrollAdapter& adapter = this->GetScrollAdapter();

    if (adapter.GetEntryCount() > 0) {
        ScrollPos spos = this->GetScrollPosition();

        size_t first = spos.firstVisibleEntryIndex;
        size_t last = first + spos.visibleEntryCount;
        int drawIndex = first;

        int minIndex = 0;
        int newIndex = this->selectedIndex - delta;
        newIndex = std::max(newIndex, minIndex);

        if (newIndex < (int)first + 1) {
            drawIndex = newIndex - 1;
        }

        drawIndex = std::max(0, drawIndex);

        this->SetSelectedIndex(newIndex);
        this->ScrollTo(drawIndex);
    }
}

void ListWindow::OnInvalidated() {
    this->Invalidated(this, this->GetSelectedIndex());
}

void ListWindow::ScrollDown(int delta) {
    IScrollAdapter& adapter = this->GetScrollAdapter();

    if (adapter.GetEntryCount() > 0) {
        ScrollPos spos = this->GetScrollPosition();

        size_t first = spos.firstVisibleEntryIndex;
        size_t last = first + spos.visibleEntryCount;
        size_t drawIndex = first;

        size_t maxIndex = adapter.GetEntryCount() - 1;
        size_t newIndex = this->selectedIndex + delta;
        newIndex = std::min(newIndex, maxIndex);

        if (newIndex >= last - 1) {
            drawIndex = drawIndex + delta;
        }

        this->SetSelectedIndex(newIndex);
        this->ScrollTo(drawIndex);
    }
}

void ListWindow::PageUp() {
    IScrollAdapter &adapter = this->GetScrollAdapter();
    ScrollPos spos = this->GetScrollPosition();
    int target = (int) this->GetPreviousPageEntryIndex();

    /* if the target position is zero, let it be so the user can see
    the top of the list. otherwise, scroll down by one to give indication
    there is more to see. */
    target = (target > 0) ? target + 1 : 0;

    this->SetSelectedIndex((target == 0) ? 0 : target + 1);
    this->ScrollTo(target);
}

void ListWindow::PageDown() {
    /* page down always makes the last item of this page, the first item
    of the next page, and selects the following item. */

    IScrollAdapter &adapter = this->GetScrollAdapter();
    ScrollPos spos = this->GetScrollPosition();

    size_t lastVisible = spos.firstVisibleEntryIndex + spos.visibleEntryCount - 1;
    this->SetSelectedIndex(std::min(adapter.GetEntryCount() - 1, lastVisible + 1));

    this->ScrollTo(lastVisible);
}

void ListWindow::ScrollTo(size_t index) {
    this->GetScrollAdapter().DrawPage(
        this, index, &this->GetScrollPosition());

    this->Repaint();
}

void ListWindow::OnSelectionChanged(size_t newIndex, size_t oldIndex) {
    /* for subclass use */
}

void ListWindow::SetSelectedIndex(size_t index) {
    if (this->selectedIndex != index) {
        size_t prev = this->selectedIndex;
        this->selectedIndex = index;
        this->OnSelectionChanged(index, prev); /* internal */
        this->SelectionChanged(this, index, prev); /* external */
    }
}

size_t ListWindow::GetSelectedIndex() {
    return this->selectedIndex;
}

void ListWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();

    size_t count = adapter->GetEntryCount();

    /* update initial state... */
    if (selectedIndex == NO_SELECTION) {
        if (adapter->GetEntryCount()) {
            this->SetSelectedIndex(0);
        }
    }
    else if (count == 0) {
        this->SetSelectedIndex(NO_SELECTION);
    }

    this->ScrollTo(this->scrollPosition.firstVisibleEntryIndex);
}

void ListWindow::OnDimensionsChanged() {
    ScrollableWindow::OnDimensionsChanged();
    this->ScrollTo(this->selectedIndex);
}

IScrollAdapter::ScrollPosition& ListWindow::GetScrollPosition() {
    this->scrollPosition.logicalIndex = this->GetSelectedIndex(); /* hack */
    return this->scrollPosition;
}
