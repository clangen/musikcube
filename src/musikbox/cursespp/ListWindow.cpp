#include <stdafx.h>
#include <algorithm>
#include "ListWindow.h"

using namespace cursespp;

typedef IScrollAdapter::ScrollPosition ScrollPos;

size_t ListWindow::NO_SELECTION = (size_t) -1;

#define REDRAW_VISIBLE_PAGE() \
    GetScrollAdapter().DrawPage( \
        this->GetContent(), \
        this->scrollPosition.firstVisibleEntryIndex, \
        &this->scrollPosition); \

ListWindow::ListWindow(IWindow *parent)
: ScrollableWindow(parent) {
    this->selectedIndex = (size_t) -1;
}

ListWindow::~ListWindow() {

}

void ListWindow::ScrollToTop() {
    this->SetSelectedIndex(0);
    this->GetScrollAdapter().DrawPage(this->GetContent(), 0, &scrollPosition);
    this->Repaint();
}

void ListWindow::ScrollToBottom() {
    IScrollAdapter& adapter = this->GetScrollAdapter();
    this->SetSelectedIndex(std::max((size_t) 0, adapter.GetEntryCount() - 1));
    adapter.DrawPage(this->GetContent(), selectedIndex, &scrollPosition);
    this->Repaint();
}

void ListWindow::Blur() {
    ScrollableWindow::Blur();
    REDRAW_VISIBLE_PAGE();
}

void ListWindow::Focus() {
    ScrollableWindow::Focus();
    REDRAW_VISIBLE_PAGE();
}

void ListWindow::ScrollUp(int delta) {
    ScrollPos spos = this->GetScrollPosition();
    IScrollAdapter& adapter = this->GetScrollAdapter();

    size_t first = spos.firstVisibleEntryIndex;
    size_t last = first + spos.visibleEntryCount;
    int drawIndex = first;

    int minIndex = 0;
    int newIndex = this->selectedIndex - delta;
    newIndex = std::max(newIndex, minIndex);

    if (newIndex < (int) first + 1) {
        drawIndex = newIndex - 1;
    }

    drawIndex = std::max(0, drawIndex);

    this->SetSelectedIndex(newIndex);

    adapter.DrawPage(this->GetContent(), drawIndex, &this->scrollPosition);

    this->Repaint();
}

void ListWindow::OnInvalidated() {
    this->Invalidated(this, this->GetSelectedIndex());
}

void ListWindow::ScrollDown(int delta) {
    ScrollPos spos = this->GetScrollPosition();
    IScrollAdapter& adapter = this->GetScrollAdapter();

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

    adapter.DrawPage(this->GetContent(), drawIndex, &this->scrollPosition);

    this->Repaint();
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

    adapter.DrawPage(this->GetContent(), target, &this->scrollPosition);
    this->Repaint();
}

void ListWindow::PageDown() {
    /* page down always makes the last item of this page, the first item
    of the next page, and selects the following item. */

    IScrollAdapter &adapter = this->GetScrollAdapter();
    ScrollPos spos = this->GetScrollPosition();

    size_t lastVisible = spos.firstVisibleEntryIndex + spos.visibleEntryCount - 1;
    this->SetSelectedIndex(std::min(adapter.GetEntryCount() - 1, lastVisible + 1));

    adapter.DrawPage(this->GetContent(), lastVisible, &this->scrollPosition);
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

    GetScrollAdapter().DrawPage(
        this->GetContent(),
        this->scrollPosition.firstVisibleEntryIndex,
        &this->scrollPosition);

    this->Repaint();
}

IScrollAdapter::ScrollPosition& ListWindow::GetScrollPosition() {
    return this->scrollPosition;
}
