#include <stdafx.h>
#include <algorithm>

#include "ScrollableWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>

using namespace cursespp;

typedef IScrollAdapter::ScrollPosition ScrollPos;

ScrollableWindow::ScrollableWindow(IWindow *parent)
: Window(parent) {
}

ScrollableWindow::~ScrollableWindow() {
}

void ScrollableWindow::OnSizeChanged() {
    Window::OnSizeChanged();
    GetScrollAdapter().SetDisplaySize(GetContentWidth(), GetContentHeight());
}

ScrollPos& ScrollableWindow::GetScrollPosition() {
    return this->scrollPosition;
}

bool ScrollableWindow::KeyPress(const std::string& key) {
    if (key == "KEY_NPAGE") { this->PageDown(); return true; }
    else if (key == "KEY_PPAGE") { this->PageUp(); return true; }
    else if (key == "KEY_DOWN") { this->ScrollDown(); return true; }
    else if (key == "KEY_UP") { this->ScrollUp(); return true; }
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

        adapter->DrawPage(
            this->GetContent(),
            pos.firstVisibleEntryIndex,
            &pos);

        this->Repaint();
    }
}

void ScrollableWindow::Show() {
    Window::Show();
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
