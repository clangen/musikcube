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
#include <algorithm>
#include <cursespp/ListWindow.h>
#include <cursespp/Scrollbar.h>

using namespace cursespp;

typedef IScrollAdapter::ScrollPosition ScrollPos;

size_t ListWindow::NO_SELECTION = (size_t) -1;

ListWindow::ListWindow(std::shared_ptr<IScrollAdapter> adapter, IWindow *parent)
: ScrollableWindow(adapter, parent)
, selectedIndex(0)
, showScrollbar(true) {

}

ListWindow::ListWindow(IWindow *parent)
: ListWindow(std::shared_ptr<IScrollAdapter>(), parent) {

}

ListWindow::~ListWindow() {

}

void ListWindow::SetScrollbarVisible(bool visible) {
    if (this->showScrollbar != visible) {
        this->showScrollbar = visible;
        this->Invalidate();
    }
}

void ListWindow::SetDecorator(Decorator decorator) {
    this->decorator = decorator;
}

void ListWindow::Invalidate() {
    this->DecorateFrame();
    Window::Invalidate();
}

void ListWindow::DecorateFrame() {
    if (this->decorator) {
        this->decorator(this);
    }

    if (this->IsFrameVisible()) {
        Scrollbar::Draw(this);
    }
}

void ListWindow::ScrollToTop() {
    this->SetSelectedIndex(0);
    this->ScrollTo(0);
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
        int drawIndex = (int) first;

        int minIndex = 0;
        int newIndex = (int) this->selectedIndex - delta;
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

bool ListWindow::IsSelectedItemCompletelyVisible() {
    IScrollAdapter& adapter = this->GetScrollAdapter();
    ScrollPos spos = this->GetScrollPosition();

    size_t first = spos.firstVisibleEntryIndex;
    size_t last = first + spos.visibleEntryCount - 1;

    if (last <= spos.logicalIndex) {
        /* get the height of all the visible items combined. */
        int sum = 0;
        for (size_t i = first; i <= spos.logicalIndex; i++) {
            sum += adapter.GetEntry(this, i)->GetLineCount();
        }

        int delta = this->GetContentHeight() - sum;

        /* special case -- the last item in the adapter is selected
        and the heights match exactly -- we're at the end! */
        if (delta == 0 && last == adapter.GetEntryCount() - 1) {
            return true;
        }

        /* compare the visible height to the actual content
        height. if the visible items are taller, and th selected
        item is the last one, it means it's partially obscured. */
        if (delta <= 0) {
            return false;
        }
    }

    return true;
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

        /* when scrolling down it's possible for the last item to be
        the selection, and partially obscured. if we hit this case, we
        just continue scrolling until the selected item is completely
        visible to the user. */
        while (!IsSelectedItemCompletelyVisible()) {
            this->ScrollTo(++drawIndex);
        }
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
        this, index, this->GetMutableScrollPosition());

    this->Invalidate();
}

void ListWindow::OnSelectionChanged(size_t newIndex, size_t oldIndex) {
    /* for subclass use */
}

bool ListWindow::IsEntryVisible(size_t index) {
    auto pos = this->GetScrollPosition();
    size_t first = pos.firstVisibleEntryIndex;
    size_t last = first + pos.visibleEntryCount;
    return (index >= first && index < last);
}

void ListWindow::SetSelectedIndex(size_t index) {
    if (this->selectedIndex != index) {
        if (index > this->GetScrollAdapter().GetEntryCount() &&
            index != NO_SELECTION)
        {
            this->selectedIndex = NO_SELECTION;
            return;
        }

        size_t prev = this->selectedIndex;
        this->selectedIndex = index;

        this->GetScrollAdapter().DrawPage(
            this,
            this->scrollPosition.firstVisibleEntryIndex,
            this->GetMutableScrollPosition());

        this->Invalidate();

        this->OnSelectionChanged(index, prev); /* internal */
        this->SelectionChanged(this, index, prev); /* external */
    }
}

size_t ListWindow::GetSelectedIndex() {
    if (this->selectedIndex >= this->GetScrollAdapter().GetEntryCount()) {
        return NO_SELECTION;
    }

    return this->selectedIndex;
}

void ListWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();

    size_t count = adapter->GetEntryCount();

    /* update initial state... */
    if (selectedIndex == NO_SELECTION) {
        if (count) {
            this->SetSelectedIndex(0);
        }
    }
    else if (count && selectedIndex >= count) {
        if (count) {
            this->SetSelectedIndex(count - 1);
        }
    }
    else if (count == 0) {
        this->SetSelectedIndex(NO_SELECTION);
    }

    this->ScrollTo(this->scrollPosition.firstVisibleEntryIndex);
}

void ListWindow::OnDimensionsChanged() {
    ScrollableWindow::OnDimensionsChanged();
    this->ScrollTo(this->GetScrollPosition().firstVisibleEntryIndex);
}

bool ListWindow::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        auto selected = this->GetSelectedIndex();
        if (selected != NO_SELECTION) {
            this->OnEntryActivated(selected);
        }
    }
    else if (key == "M-enter") {
        auto selected = this->GetSelectedIndex();
        if (selected != NO_SELECTION) {
            this->OnEntryContextMenu(selected);
        }
    }
    return ScrollableWindow::KeyPress(key);
}

bool ListWindow::MouseEvent(const IMouseHandler::Event& event) {
    /* CAL TODO: this method assumes each row is a single cell tall. */
    bool result = ScrollableWindow::MouseEvent(event);

    auto first = this->scrollPosition.firstVisibleEntryIndex;

    if (first == NO_SELECTION) {
        return result;
    }

    size_t offset = first + (size_t) event.y;

    if (offset < this->GetScrollAdapter().GetEntryCount()) {
        if (event.Button1Clicked()) {
            this->SetSelectedIndex(offset);
        }
        if (event.Button3Clicked()) {
            this->SetSelectedIndex(offset);
            this->OnEntryContextMenu(offset);
        }
        else if (event.Button1DoubleClicked()) {
            this->FocusInParent();
            this->SetSelectedIndex(offset);
            this->OnEntryActivated(offset); /* internal */
        }
    }

    return result;
}

void ListWindow::OnEntryActivated(size_t index) {
    this->EntryActivated(this, index); /* external */
}

void ListWindow::OnEntryContextMenu(size_t index) {
    this->EntryContextMenu(this, index); /* external */
}

IScrollAdapter::ScrollPosition& ListWindow::GetMutableScrollPosition() {
    this->scrollPosition.logicalIndex = this->GetSelectedIndex(); /* hack */
    return this->scrollPosition;
}

const IScrollAdapter::ScrollPosition& ListWindow::GetScrollPosition() {
    return this->GetMutableScrollPosition();
}
