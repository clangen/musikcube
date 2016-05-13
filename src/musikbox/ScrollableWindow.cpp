#pragma once

#include "stdafx.h"
#include "ScrollableWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>

typedef IScrollAdapter::ScrollPosition ScrollPos;

ScrollableWindow::ScrollableWindow()
: Window() {
}

ScrollableWindow::~ScrollableWindow() {
}

void ScrollableWindow::SetSize(int width, int height) {
    Window::SetSize(width, height);
    GetScrollAdapter().SetDisplaySize(GetContentWidth(), GetContentHeight());
}

ScrollPos& ScrollableWindow::GetScrollPosition() {
    return this->scrollPosition;
}

void ScrollableWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();

    if (IsLastItemVisible()) {
        this->ScrollToBottom();
    }
    else {
        ScrollPos &pos = this->GetScrollPosition();

        GetScrollAdapter().DrawPage(
            this->GetContent(), 
            pos.firstVisibleEntryIndex, 
            &pos);

        this->Repaint();
    }
}

void ScrollableWindow::Create() {
    Window::Create();
    this->OnAdapterChanged();
}

void ScrollableWindow::ScrollToTop() {
    GetScrollAdapter().DrawPage(this->GetContent(), 0, &this->GetScrollPosition());
    this->Repaint();
}

void ScrollableWindow::ScrollToBottom() {
    GetScrollAdapter().DrawPage(
        this->GetContent(), 
        GetScrollAdapter().GetEntryCount(),
        &this->GetScrollPosition());

    this->Repaint();
}

void ScrollableWindow::ScrollUp(int delta) {
    ScrollPos &pos = this->GetScrollPosition();

    if (pos.firstVisibleEntryIndex > 0) {
        GetScrollAdapter().DrawPage(
            this->GetContent(),
            pos.firstVisibleEntryIndex - delta,
            &pos);

        this->Repaint();
    }
}

void ScrollableWindow::ScrollDown(int delta) {
    ScrollPos &pos = this->GetScrollPosition();

    GetScrollAdapter().DrawPage(
        this->GetContent(),
        pos.firstVisibleEntryIndex + delta,
        &pos);

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

    return max(0, i);
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