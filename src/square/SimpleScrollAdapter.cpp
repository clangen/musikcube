#include "stdafx.h"

#include "SimpleScrollAdapter.h"
#include <boost/algorithm/string.hpp>

SimpleScrollAdapter::SimpleScrollAdapter() {
    this->lineCount = 0;
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

size_t SimpleScrollAdapter::GetLineCount(size_t width) {
    return this->lineCount;
}

size_t SimpleScrollAdapter::GetEntryCount() {
    return this->entries.size();
}

void SimpleScrollAdapter::DrawPage(WINDOW* window, size_t lineNumber) {
    wclear(window);

    /* binary search to find where we need to start */

    size_t offset = this->FindEntryIndex(lineNumber);
    Iterator it = this->entries.begin() + offset;

    /* if found, the iterator will be pointing at the first visible
    element. */

    Iterator end = this->entries.end();
    size_t remaining = this->height;
    size_t w = this->width;
    size_t c = lineNumber - (*it)->GetIndex();

    do {
        size_t count = (*it)->GetLineCount(w);

        for (size_t i = c; i < count; i++) {
            wprintw(window, "%s\n", (*it)->GetLine(i, w).c_str());
            --remaining;
        }

        ++it;
        c = 0;
    } while (it != end && remaining != 0);

}

void SimpleScrollAdapter::AddLine(const std::string& str) {
    std::vector<std::string> lines;
    boost::algorithm::split(lines, str, boost::is_any_of("\n"));

    for (size_t i = 0; i < lines.size(); i++) {
        boost::shared_ptr<Entry> entry(new Entry(lines[i]));
        entry->SetIndex(this->lineCount++);
        entries.push_back(entry);
    }
}

size_t SimpleScrollAdapter::FindEntryIndex(int lineNumber) {
    if (lineCount == -1) {
        Reindex();
    }

    size_t min = 0, max = this->entries.size();

    while (true) {
        size_t guess = (min + max) / 2;

        Entry* entry = this->entries.at(guess).get();
        size_t first = entry->GetIndex();
        size_t last = first + entry->GetLineCount(this->width);
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
        index += (*it)->GetLineCount(this->width);
    }
}

SimpleScrollAdapter::Entry::Entry(const std::string& value) {
    this->value = value;
    this->charCount = value.size();
}

size_t SimpleScrollAdapter::Entry::GetLineCount(size_t width) {
    int full = (charCount / width);
    int remain = (charCount % width == 0 ? 0 : 1);
    return max(1, full + remain);
}

std::string SimpleScrollAdapter::Entry::GetLine(size_t n, size_t width) {
    size_t start = n * width;
    size_t count = start + width;
    count = (count > this->value.size()) ? std::string::npos : count;
    return this->value.substr(start, count);
}

std::string SimpleScrollAdapter::Entry::GetValue() {
    return value;
}

size_t SimpleScrollAdapter::Entry::GetIndex() {
    return this->index;
}

void SimpleScrollAdapter::Entry::SetIndex(size_t index) {
    this->index = index;
}

