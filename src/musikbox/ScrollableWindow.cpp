#pragma once

#include "stdafx.h"
#include "ScrollableWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>

ScrollableWindow::ScrollableWindow()
: Window() {
}

ScrollableWindow::~ScrollableWindow() {
}

void ScrollableWindow::SetSize(int width, int height) {
    Window::SetSize(width, height);
    GetScrollAdapter().SetDisplaySize(GetContentWidth(), GetContentHeight());
}

void ScrollableWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();
    if (IsLastItemVisible()) {
        this->ScrollToBottom();
    }
    else {
        GetScrollAdapter().DrawPage(
            this->GetContent(), 
            this->scrollPosition.firstVisibleEntryIndex, 
            &this->scrollPosition);

        this->Repaint();
    }
}

void ScrollableWindow::Create() {
    Window::Create();
    this->OnAdapterChanged();
}

void ScrollableWindow::ScrollToTop() {
    GetScrollAdapter().DrawPage(this->GetContent(), 0, &scrollPosition);
    this->Repaint();
}

void ScrollableWindow::ScrollToBottom() {
    GetScrollAdapter().DrawPage(
        this->GetContent(), 
        GetScrollAdapter().GetEntryCount(),
        &scrollPosition);

    this->Repaint();
}

void ScrollableWindow::ScrollUp(int delta) {
    if (this->scrollPosition.firstVisibleEntryIndex > 0) {
        GetScrollAdapter().DrawPage(
            this->GetContent(),
            this->scrollPosition.firstVisibleEntryIndex - delta,
            &scrollPosition);

        this->Repaint();
    }
}

void ScrollableWindow::ScrollDown(int delta) {
    GetScrollAdapter().DrawPage(
        this->GetContent(),
        this->scrollPosition.firstVisibleEntryIndex + delta,
        &scrollPosition);

    this->Repaint();
}

size_t ScrollableWindow::GetPreviousPageEntryIndex() {
    IScrollAdapter* adapter = &GetScrollAdapter();
    int remaining = this->GetContentHeight();
    int width = this->GetContentWidth();

    int i = this->scrollPosition.firstVisibleEntryIndex;
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
        this->scrollPosition.firstVisibleEntryIndex -
        GetPreviousPageEntryIndex());
}

void ScrollableWindow::PageDown() {
    ScrollDown(this->scrollPosition.visibleEntryCount - 1);
}

bool ScrollableWindow::IsLastItemVisible() {
    size_t lastIndex = this->scrollPosition.totalEntries;
    lastIndex = (lastIndex == 0) ? lastIndex : lastIndex - 1;

    size_t firstVisible = this->scrollPosition.firstVisibleEntryIndex;
    size_t lastVisible = firstVisible + this->scrollPosition.visibleEntryCount;
    return lastIndex >= firstVisible && lastIndex <= lastVisible;
}