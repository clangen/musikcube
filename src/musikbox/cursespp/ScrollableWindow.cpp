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

#include "ScrollableWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>

using namespace cursespp;

static const size_t INVALID_INDEX = (size_t) -1;

typedef IScrollAdapter::ScrollPosition ScrollPos;

#define REDRAW_VISIBLE_PAGE() \
    { \
        ScrollPos& pos = GetScrollPosition(); \
        GetScrollAdapter().DrawPage( \
            this, \
            pos.firstVisibleEntryIndex, \
            &pos); \
    } \

ScrollableWindow::ScrollableWindow(IWindow *parent)
: Window(parent) {
}

ScrollableWindow::~ScrollableWindow() {
}

void ScrollableWindow::OnDimensionsChanged() {
    Window::OnDimensionsChanged();

    IScrollAdapter& adapter = this->GetScrollAdapter();
    ScrollPos& pos = this->GetScrollPosition();

    adapter.SetDisplaySize(
        this->GetContentWidth(),
        this->GetContentHeight());
}

ScrollPos& ScrollableWindow::GetScrollPosition() {
    return this->scrollPosition;
}

bool ScrollableWindow::KeyPress(const std::string& key) {
    /* note we allow KEY_DOWN and KEY_UP to continue to propagate if
    the logical (selected) index doesn't actually change -- i.e. the
    user is at the beginning or end of the scrollable area. this is so
    controllers can change focus in response to UP/DOWN if necessary. */

    if (key == "KEY_NPAGE") { this->PageDown(); return true; }
    else if (key == "KEY_PPAGE") { this->PageUp(); return true; }
    else if (key == "KEY_DOWN") { 
        const size_t before = this->GetScrollPosition().logicalIndex;
        this->ScrollDown();
        const size_t after = this->GetScrollPosition().logicalIndex;
        return (before != after);
    }
    else if (key == "KEY_UP") { 
        const size_t before = this->GetScrollPosition().logicalIndex;
        this->ScrollUp();
        const size_t after = this->GetScrollPosition().logicalIndex;
        return (before != after);
    }
    else if (key == "KEY_HOME") { this->ScrollToTop(); return true; }
    else if (key == "KEY_END") { this->ScrollToBottom(); return true; }
    return false;
}

void ScrollableWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();

    adapter->SetDisplaySize(GetContentWidth(), GetContentHeight());

    if (IsLastItemVisible()) {
        this->ScrollToBottom();
    }
    else {
        ScrollPos &pos = this->GetScrollPosition();
        adapter->DrawPage(this, pos.firstVisibleEntryIndex, &pos);
        this->Repaint();
    }
}

void ScrollableWindow::Show() {
    Window::Show();
    this->OnAdapterChanged();
}

void ScrollableWindow::ScrollToTop() {
    GetScrollAdapter().DrawPage(this, 0, &this->GetScrollPosition());
    this->Repaint();
}

void ScrollableWindow::ScrollToBottom() {
    GetScrollAdapter().DrawPage(
        this,
        GetScrollAdapter().GetEntryCount(),
        &this->GetScrollPosition());

    this->Repaint();
}

void ScrollableWindow::ScrollUp(int delta) {
    ScrollPos &pos = this->GetScrollPosition();

    if (pos.firstVisibleEntryIndex > 0) {
        GetScrollAdapter().DrawPage(
            this, pos.firstVisibleEntryIndex - delta, &pos);

        this->Repaint();
    }
}

void ScrollableWindow::ScrollDown(int delta) {
    ScrollPos &pos = this->GetScrollPosition();

    GetScrollAdapter().DrawPage(
        this, pos.firstVisibleEntryIndex + delta, &pos);

    this->Repaint();
}

size_t ScrollableWindow::GetPreviousPageEntryIndex() {
    IScrollAdapter* adapter = &GetScrollAdapter();
    int remaining = this->GetContentHeight();
    int width = this->GetContentWidth();

    int i = this->GetScrollPosition().firstVisibleEntryIndex;
    while (i >= 0) {
        IScrollAdapter::EntryPtr entry = adapter->GetEntry(i);
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
    ScrollPos &pos = this->GetScrollPosition();

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
