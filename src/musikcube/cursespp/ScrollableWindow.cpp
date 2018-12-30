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

#include <cursespp/ScrollableWindow.h>
#include <cursespp/Screen.h>
#include <cursespp/Colors.h>

using namespace cursespp;

static const size_t INVALID_INDEX = (size_t) -1;

typedef IScrollAdapter::ScrollPosition ScrollPos;

class EmptyAdapter : public IScrollAdapter {
public:
    virtual void SetDisplaySize(size_t width, size_t height) { }
    virtual size_t GetEntryCount() { return 0; }
    virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) { return IScrollAdapter::EntryPtr(); }
    virtual void DrawPage(ScrollableWindow* window, size_t index, ScrollPosition& result) { }
};

static EmptyAdapter emptyAdapter;

#define REDRAW_VISIBLE_PAGE() \
    { \
        ScrollPos& pos = GetMutableScrollPosition(); \
        GetScrollAdapter().DrawPage( \
            this, \
            pos.firstVisibleEntryIndex, \
            pos); \
    } \

ScrollableWindow::ScrollableWindow(std::shared_ptr<IScrollAdapter> adapter, IWindow *parent)
: Window(parent)
, adapter(adapter)
, allowArrowKeyPropagation(false) {
}

ScrollableWindow::ScrollableWindow(IWindow *parent)
: ScrollableWindow(std::shared_ptr<IScrollAdapter>(), parent) {
}

ScrollableWindow::~ScrollableWindow() {
}

void ScrollableWindow::SetAdapter(std::shared_ptr<IScrollAdapter> adapter) {
    if (adapter != this->adapter) {
        this->adapter = adapter;
        this->ScrollToTop();
    }
}

IScrollAdapter& ScrollableWindow::GetScrollAdapter() {
    if (this->adapter) {
        return *this->adapter;
    }

    return emptyAdapter;
}

void ScrollableWindow::OnDimensionsChanged() {
    Window::OnDimensionsChanged();

    IScrollAdapter& adapter = this->GetScrollAdapter();

    adapter.SetDisplaySize(
        this->GetContentWidth(),
        this->GetContentHeight());
}

ScrollPos& ScrollableWindow::GetMutableScrollPosition() {
    return this->scrollPosition;
}

const ScrollPos& ScrollableWindow::GetScrollPosition() {
    return this->scrollPosition;
}

void ScrollableWindow::SetAllowArrowKeyPropagation(bool allow) {
    this->allowArrowKeyPropagation = allow;
}

bool ScrollableWindow::KeyPress(const std::string& key) {
    /* note we allow KEY_DOWN and KEY_UP to continue to propagate if
    the logical (selected) index doesn't actually change -- i.e. the
    user is at the beginning or end of the scrollable area. this is so
    controllers can change focus in response to UP/DOWN if necessary. */
    auto& keys = NavigationKeys();

    if (keys.PageDown(key)) { this->PageDown(); return true; }
    else if (keys.PageUp(key)) { this->PageUp(); return true; }
    else if (keys.Down(key)) {
        const size_t before = this->GetScrollPosition().logicalIndex;
        this->ScrollDown();
        const size_t after = this->GetScrollPosition().logicalIndex;
        return !this->allowArrowKeyPropagation || (before != after);
    }
    else if (keys.Up(key)) {
        const size_t before = this->GetScrollPosition().logicalIndex;
        this->ScrollUp();
        const size_t after = this->GetScrollPosition().logicalIndex;
        return !this->allowArrowKeyPropagation || (before != after);
    }
    else if (keys.Home(key)) { this->ScrollToTop(); return true; }
    else if (keys.End(key)) { this->ScrollToBottom(); return true; }
    return false;
}

bool ScrollableWindow::MouseEvent(const IMouseHandler::Event& event) {
    if (event.Button1Clicked()) {
        this->FocusInParent();
        return true;
    }
    else if (event.MouseWheelDown()) {
        this->PageDown();
        return true;
    }
    else if (event.MouseWheelUp()) {
        this->PageUp();
        return true;
    }
    return false;
}


void ScrollableWindow::OnRedraw() {
    IScrollAdapter *adapter = &GetScrollAdapter();
    ScrollPos &pos = this->GetMutableScrollPosition();
    adapter->DrawPage(this, pos.firstVisibleEntryIndex, pos);
    this->Invalidate();
}

void ScrollableWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();

    adapter->SetDisplaySize(GetContentWidth(), GetContentHeight());

    if (IsLastItemVisible()) {
        this->ScrollToBottom();
    }
    else {
        ScrollPos &pos = this->GetMutableScrollPosition();
        adapter->DrawPage(this, pos.firstVisibleEntryIndex, pos);
        this->Invalidate();
    }
}

void ScrollableWindow::Show() {
    Window::Show();
    this->OnAdapterChanged();
}

void ScrollableWindow::ScrollToTop() {
    GetScrollAdapter().DrawPage(this, 0, this->GetMutableScrollPosition());
    this->Invalidate();
}

void ScrollableWindow::ScrollToBottom() {
    GetScrollAdapter().DrawPage(
        this,
        GetScrollAdapter().GetEntryCount(),
        this->GetMutableScrollPosition());

    this->Invalidate();
}

void ScrollableWindow::ScrollUp(int delta) {
    ScrollPos &pos = this->GetMutableScrollPosition();

    if (pos.firstVisibleEntryIndex > 0) {
        GetScrollAdapter().DrawPage(
            this, pos.firstVisibleEntryIndex - delta, pos);

        this->Invalidate();
    }
}

void ScrollableWindow::ScrollDown(int delta) {
    ScrollPos &pos = this->GetMutableScrollPosition();

    GetScrollAdapter().DrawPage(
        this, pos.firstVisibleEntryIndex + delta, pos);

    this->Invalidate();
}

size_t ScrollableWindow::GetPreviousPageEntryIndex() {
    IScrollAdapter* adapter = &GetScrollAdapter();
    int remaining = this->GetContentHeight();
    int width = this->GetContentWidth();

    int i = this->GetScrollPosition().firstVisibleEntryIndex;
    while (i >= 0) {
        IScrollAdapter::EntryPtr entry = adapter->GetEntry(this, i);
        entry->SetWidth(width);

        int count = entry->GetLineCount();
        if (count > remaining) {
            break;
        }

        i--;
        remaining -= count;
    }

    return std::max(0, i);
}

void ScrollableWindow::PageUp() {
    ScrollUp(
        this->GetScrollPosition().firstVisibleEntryIndex -
        GetPreviousPageEntryIndex());
}

void ScrollableWindow::PageDown() {
    ScrollDown(this->GetScrollPosition().visibleEntryCount - 1);
}

bool ScrollableWindow::IsLastItemVisible() {
    ScrollPos &pos = this->GetMutableScrollPosition();

    size_t lastIndex = pos.totalEntries;
    lastIndex = (lastIndex == 0) ? lastIndex : lastIndex - 1;

    size_t firstVisible = pos.firstVisibleEntryIndex;
    size_t lastVisible = firstVisible + pos.visibleEntryCount;
    return lastIndex >= firstVisible && lastIndex <= lastVisible;
}

void ScrollableWindow::Blur() {
    Window::Blur();
    REDRAW_VISIBLE_PAGE();
}

void ScrollableWindow::Focus() {
    Window::Focus();
    REDRAW_VISIBLE_PAGE();
}
