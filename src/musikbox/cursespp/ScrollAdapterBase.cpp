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
#include "ScrollAdapterBase.h"
#include "ScrollableWindow.h"
#include "MultiLineEntry.h"

using namespace cursespp;

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
    for (int i = (int) desired; i < end && totalHeight > 0; i++) {
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

void ScrollAdapterBase::DrawPage(ScrollableWindow* scrollable, size_t index, ScrollPosition *result) {
    if (result != NULL) {
        result->visibleEntryCount = 0;
        result->firstVisibleEntryIndex = 0;
        result->lineCount = 0;
        result->totalEntries = 0;
        result->logicalIndex = 0;
    }

    WINDOW* window = scrollable->GetContent();

    werase(window);

    if (this->height == 0 || this->width == 0 || this->GetEntryCount() == 0) {
        return;
    }

    if (index >= GetEntryCount()) {
        index = GetEntryCount() - 1;
    }

    /* unfortunately this needs to go here so the GetEntry() method knows
    what the the implied focus is */
    result->logicalIndex = index;

    std::deque<EntryPtr> visible;
    size_t topIndex; /* calculated by GetVisibleItems */
    GetVisibleItems(index, visible, topIndex);

    size_t drawnLines = 0;

    for (size_t e = 0; e < visible.size(); e++) {
        EntryPtr entry = visible.at(e);
        size_t count = entry->GetLineCount();

        for (size_t i = 0; i < count && drawnLines < this->height; i++) {
            int64 attrs = -1;

            if (this->decorator) {
                attrs = this->decorator(scrollable, e, entry);
            }

            if (attrs == -1) {
                attrs = entry->GetAttrs(i);
            }

            if (attrs != -1) {
                wattron(window, attrs);
            }

            std::string line = entry->GetLine(i);
            size_t len = u8len(line);

            /* pad with empty spaces to the end of the line. this allows us to
            do highlight rows. this should probably be configurable. */

            int remain = this->width - len;
            if (remain > 0) {
                line += std::string(remain, ' ');
            }

            /* string is padded above, we don't need a \n */

            wprintw(window, "%s", line.c_str());

            if (attrs != -1) {
                wattroff(window, attrs);
            }

            ++drawnLines;
        }
    }

    if (result != NULL) {
        result->visibleEntryCount = visible.size();
        result->firstVisibleEntryIndex = topIndex;
        result->lineCount = drawnLines;
        result->totalEntries = GetEntryCount();
        result->logicalIndex = index;
    }
}
