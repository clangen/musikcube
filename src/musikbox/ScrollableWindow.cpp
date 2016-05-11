#pragma once

#include "stdafx.h"
#include "ScrollableWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>

ScrollableWindow::ScrollableWindow()
: Window() {
    scrollPosition = 0;
    scrolledToBottom = true;
}

ScrollableWindow::~ScrollableWindow() {
}

void ScrollableWindow::OnAdapterChanged() {
    IScrollAdapter *adapter = &GetScrollAdapter();
    if (scrolledToBottom) {
        this->ScrollToBottom();
    }
    else {
        GetScrollAdapter().DrawPage(this->GetContent(), this->scrollPosition);
        this->Repaint();
    }
}

void ScrollableWindow::Create() {
    Window::Create();
    this->OnAdapterChanged();
}

size_t ScrollableWindow::GetFirstVisible() {
    return scrollPosition;
}

size_t ScrollableWindow::GetLastVisible() {
    size_t total = GetScrollAdapter().GetLineCount();
    return min(scrollPosition + this->GetContentHeight(), total);
}

void ScrollableWindow::ScrollToTop() {
    GetScrollAdapter().DrawPage(this->GetContent(), 0);
    this->scrollPosition = 0;
    this->Repaint();
    this->CheckScrolledToBottom();
}

void ScrollableWindow::ScrollToBottom() {
    IScrollAdapter *adapter = &GetScrollAdapter();

    int total = (int) adapter->GetLineCount();
    int height = this->GetContentHeight();

    int actual = total - height;
    actual = (actual < 0) ? 0 : actual;

    adapter->DrawPage(this->GetContent(), actual);

    this->scrollPosition = actual;
    this->Repaint();
    this->CheckScrolledToBottom();
}

void ScrollableWindow::ScrollUp(int delta) {
    int actual = (int) this->scrollPosition - delta;
    actual = (actual < 0) ? 0 : actual;

    GetScrollAdapter().DrawPage(this->GetContent(), actual);

    this->scrollPosition = (size_t) actual;
    this->Repaint();
    this->CheckScrolledToBottom();
}

void ScrollableWindow::ScrollDown(int delta) {
    IScrollAdapter *adapter = &GetScrollAdapter();

    int total = adapter->GetLineCount();
    int height = this->GetContentHeight();
    int optimal = total - height;
    int max = max(0, optimal);

    int actual = (int) this->scrollPosition + delta;
    actual = (actual > max) ? max : actual;

    adapter->DrawPage(this->GetContent(), actual);

    this->scrollPosition = (size_t) actual;
    this->Repaint();
    this->CheckScrolledToBottom();
}

void ScrollableWindow::PageUp() {
    ScrollUp(this->GetContentHeight() - 1);
}

void ScrollableWindow::PageDown() {
    ScrollDown(this->GetContentHeight() - 1);
}

void ScrollableWindow::CheckScrolledToBottom() {

}