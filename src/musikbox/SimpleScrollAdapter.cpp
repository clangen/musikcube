#include "stdafx.h"

#include "SimpleScrollAdapter.h"
#include "SingleLineEntry.h"
#include "MultiLineEntry.h"
#include <boost/algorithm/string.hpp>
#include <utf8/utf8/unchecked.h>

#define MAX_ENTRY_COUNT 0xffffffff

SimpleScrollAdapter::SimpleScrollAdapter() {
    this->lineCount = 0;

    /* the adapters can have a maximum size. as we remove elements from
    the back, we don't want to re-index everything. instead, we'll use
    this offset for future calculations when searching for items. */
    this->removedOffset = 0; 
    this->maxEntries = MAX_ENTRY_COUNT;
}

SimpleScrollAdapter::~SimpleScrollAdapter() {

}

void SimpleScrollAdapter::SetDisplaySize(size_t width, size_t height) {
    if (height != this->height || width != this->width) {
        this->height = height;
        this->width = width;
        Reindex();
    }
}

size_t SimpleScrollAdapter::GetLineCount() {
    return this->lineCount;
}

size_t SimpleScrollAdapter::GetEntryCount() {
    return this->entries.size();
}

void SimpleScrollAdapter::SetMaxEntries(size_t maxEntries) {
    this->maxEntries = maxEntries;
}

void SimpleScrollAdapter::DrawPage(WINDOW* window, size_t lineNumber) {
    wclear(window);

    if (this->lineCount <= 0) {
        return;
    }

    if (lineNumber >= this->lineCount) {
        lineNumber = this->lineCount - 1;
    }

    if (lineNumber < 0) {
        lineNumber = 0;
    }

    /* binary search to find where we need to start */

    size_t offset = this->FindEntryIndex(lineNumber);
    Iterator it = this->entries.begin() + offset;

    /* if found, the iterator will be pointing at the first visible
    element. */

    Iterator end = this->entries.end();
    size_t remaining = this->height;
    size_t w = this->width;
    size_t c = lineNumber - ((*it)->GetIndex() - removedOffset);

    do {
        size_t count = (*it)->GetLineCount();

        int64 attrs = (*it)->GetAttrs();
        if (attrs != -1) {
            wattron(window, attrs);
        }

        for (size_t i = c; i < count && remaining != 0; i++) {
            std::string line = (*it)->GetLine(i).c_str();
            size_t len = u8len(line);

            /* don't add a newline if we're going to hit the end of the line, the
            newline will be added automatically. */
            wprintw(window, "%s%s", line.c_str(), len >= this->width ? "" : "\n");

            --remaining;
        }

        if (attrs != -1) {
            wattroff(window, attrs);
        }

        ++it;
        c = 0;
    } while (it != end && remaining != 0);

}

void SimpleScrollAdapter::AddEntry(boost::shared_ptr<IEntry> entry) {
    entry->SetWidth(this->width);
    entry->SetIndex(this->lineCount + this->removedOffset);
    entries.push_back(entry);
    this->lineCount += entry->GetLineCount();

    while (entries.size() > this->maxEntries) {
        boost::shared_ptr<IEntry> e = entries.front();
        size_t lineCount = e->GetLineCount();
        this->removedOffset += lineCount;
        this->lineCount -= lineCount;
        entries.pop_front();
    }
}

size_t SimpleScrollAdapter::FindEntryIndex(size_t lineNumber) {
    if (lineCount == -1) {
        Reindex();
    }

    size_t min = 0, max = this->entries.size() - 1;

    while (true) {
        size_t guess = (min + max) / 2;

        IEntry* entry = this->entries.at(guess).get();
        size_t first = entry->GetIndex() - this->removedOffset;
        size_t last = first + entry->GetLineCount();
        if (lineNumber >= first && lineNumber <= last) {
            return guess;
        }
        else if (lineNumber > first) { /* guess too low */
            min = guess + 1;
        }
        else if (lineNumber < last) { /* guess too high */
            max = guess - 1;
        }
    }
}

void SimpleScrollAdapter::Reindex() {
    int index = 0;

    for (Iterator it = this->entries.begin(); it != this->entries.end(); it++) {
        (*it)->SetIndex(index);
        (*it)->SetWidth(this->width);
        index += (*it)->GetLineCount();
    }

    this->removedOffset = 0;
    this->lineCount = index;
}
