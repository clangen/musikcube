#include "stdafx.h"
#include "ScrollAdapterBase.h"
#include "MultiLineEntry.h"

typedef IScrollAdapter::EntryPtr EntryPtr;

ScrollAdapterBase::ScrollAdapterBase() {
    this->height = 0;
    this->width = 0;
}

ScrollAdapterBase::~ScrollAdapterBase() {

}

void ScrollAdapterBase::SetDisplaySize(size_t width, size_t height) {
    this->width = width;
    this->height = height;
}

size_t ScrollAdapterBase::GetLineCount() {
    return -1;
}

void ScrollAdapterBase::GetVisibleItems(
    size_t desired, std::deque<EntryPtr>& target, size_t& start)
{
    size_t actual = desired;

    /* ensure we have enough data to draw from the specified position
    to the end. if we don't try to back up a bit until we can fill
    the buffer */
    int totalHeight = (int) this->height;

    /* forward search first... */
    int end = (int) GetEntryCount();
    for (int i = (int) desired; i < end && totalHeight >= 0; i++) {
        EntryPtr entry = this->GetEntry(i);
        entry->SetWidth(this->width);
        totalHeight -= entry->GetLineCount();
        target.push_back(entry);
    }

    if (totalHeight > 0) {
        target.clear();

        /* oops, let's move backwards from the end */
        totalHeight = this->height;
        int i = GetEntryCount() - 1;
        while (i >= 0 && totalHeight >= 0) {
            EntryPtr entry = this->GetEntry(i);
            entry->SetWidth(this->width);

            int lines = entry->GetLineCount();
            if (lines > totalHeight) {
                break; /* this Entry won't fit. bail. */
            }

            totalHeight -= lines;
            target.push_front(entry);
            --i;
        }

        actual = i + 1;
    }

    start = actual;
}

void ScrollAdapterBase::DrawPage(WINDOW* window, size_t index, ScrollPosition *result) {
    if (result != NULL) {
        result->visibleEntryCount = 0;
        result->firstVisibleEntryIndex = 0;
        result->lineCount = 0;
        result->totalEntries = 0;
    }

    wclear(window);

    if (this->height == 0 || this->width == 0 || this->GetEntryCount() == 0) {
        return;
    }

    if (index >= GetEntryCount()) {
        index = GetEntryCount() - 1;
    }

    std::deque<EntryPtr> visible;
    size_t topIndex; /* calculated by GetVisibleItems */
    GetVisibleItems(index, visible, topIndex);

    size_t drawnLines = 0;

    for (size_t e = 0; e < visible.size(); e++) {
        EntryPtr entry = visible.at(e);
        size_t count = entry->GetLineCount();
        int64 attrs = entry->GetAttrs();

        if (attrs != -1) {
            wattron(window, attrs);
        }

        for (size_t i = 0; i < count && drawnLines < this->height; i++) {
            std::string line = entry->GetLine(i);
            size_t len = u8len(line);

            /* don't add a newline if we're going to hit the end of the line, the
            newline will be added automatically. */
            wprintw(window, "%s%s", line.c_str(), len >= this->width ? "" : "\n");

            ++drawnLines;
        }

        if (attrs != -1) {
            wattroff(window, attrs);
        }
    }

    if (result != NULL) {
        result->visibleEntryCount = visible.size();
        result->firstVisibleEntryIndex = topIndex;
        result->lineCount = drawnLines;
        result->totalEntries = GetEntryCount();
    }
}