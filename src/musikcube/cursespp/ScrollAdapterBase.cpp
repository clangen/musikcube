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
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/ScrollableWindow.h>
#include <cursespp/MultiLineEntry.h>
#include <cursespp/ListWindow.h>

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

size_t ScrollAdapterBase::GetVisibleItems(
    cursespp::ScrollableWindow* window,
    size_t desiredTopIndex,
    std::deque<EntryPtr>& target)
{
    size_t actualTopIndex = desiredTopIndex;

    /* ensure we have enough data to draw from the specified position
    to the end. if we don't try to back up a bit until we can fill
    the buffer */
    int totalHeight = (int) this->height;
    int entryCount = (int) this->GetEntryCount();

    /* we assume the general case -- we're some where in the middle of the
    list. we'll start from the specified first item and work our way down */
    for (int i = (int) desiredTopIndex; i < entryCount && totalHeight > 0; i++) {
        EntryPtr entry = this->GetEntry(window, i);
        entry->SetWidth(this->width);
        totalHeight -= entry->GetLineCount();
        target.push_back(entry);
    }

    /* however, if the list is short, we can actually draw more items above
    the specified one. let's work our way backwards! */
    if (totalHeight > 0) {
        target.clear();

        totalHeight = this->height;
        int i = GetEntryCount() - 1;
        while (i >= 0 && totalHeight >= 0) {
            EntryPtr entry = this->GetEntry(window, i);
            entry->SetWidth(this->width);

            int lines = entry->GetLineCount();
            if (lines > totalHeight) {
                break; /* this Entry won't fit. bail. */
            }

            totalHeight -= lines;
            target.push_front(entry);
            --i;
        }

        actualTopIndex = i + 1;
    }

    return actualTopIndex;
}

void ScrollAdapterBase::DrawPage(ScrollableWindow* scrollable, size_t index, ScrollPosition& result) {
    WINDOW* window = scrollable->GetContent();
    werase(window);

    if (!scrollable->IsVisible() || !window || this->height == 0 || this->width == 0 || this->GetEntryCount() == 0) {
        return;
    }

    if (index >= GetEntryCount()) {
        index = GetEntryCount() - 1;
    }

    std::deque<EntryPtr> visible;
    size_t topIndex = GetVisibleItems(scrollable, index, visible);

    size_t drawnLines = 0;

    for (size_t e = 0; e < visible.size(); e++) {
        EntryPtr entry = visible.at(e);
        size_t count = entry->GetLineCount();

        for (size_t i = 0; i < count && drawnLines < this->height; i++) {
            Color attrs = Color::Default;

            if (this->decorator) {
                attrs = this->decorator(scrollable, topIndex + e, i, entry);
            }

            if (attrs == -1) {
                attrs = entry->GetAttrs(i);
            }

            if (attrs != -1) {
                wattron(window, attrs);
            }

            std::string line = entry->GetLine(i);
            size_t len = u8cols(line);

            /* pad with empty spaces to the end of the line. this allows us to
            do highlight rows. this should probably be configurable. */

            int remain = this->width - len;
            if (remain > 0) {
                line += std::string(remain, ' ');
            }

            /* string is padded above, we don't need a \n */

            checked_wprintw(window, "%s", line.c_str());

            if (attrs != -1) {
                wattroff(window, attrs);
            }

            ++drawnLines;
        }
    }

    result.visibleEntryCount = visible.size();
    result.firstVisibleEntryIndex = topIndex;
    result.lineCount = drawnLines;
    result.totalEntries = GetEntryCount();
}
