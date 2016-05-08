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
        this->lineCount = -1;
    }
}

size_t SimpleScrollAdapter::GetLineCount(size_t width) {
    if (this->lineCount == -1) {
        Iterator it = this->entries.begin();
        for ( ; it != entries.end(); it++) {
            this->lineCount += (*it)->GetLineCount(width);
        }
    }

    return this->lineCount;
}

size_t SimpleScrollAdapter::GetEntryCount() {
    return this->entries.size();
}

void SimpleScrollAdapter::DrawPage(WINDOW* window, size_t lineNumber) {
    wclear(window);

    /* find the entry at the specified line number. this is really inefficient,
    and runs in O(n) time. need to figure out a way to speed this up. */

    int line = 0, curr = 0;
    bool found = false;
    Iterator it = this->entries.begin();
    for ( ; it != this->entries.end(); it++) {
        curr = (*it)->GetLineCount(this->width);

        if (line + curr > lineNumber) {
            found = true;
            break;
        }
        else {
            line += curr;
        }
    }

    /* if found, the iterator will be pointing at the first visible
    element. */

    if (found) {
        Iterator end = this->entries.end();
        size_t remaining = this->height;
        size_t w = this->width;
        size_t c = lineNumber - line;

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

}

void SimpleScrollAdapter::AddLine(const std::string& str) {
    lineCount = -1;

    std::vector<std::string> lines;
    boost::algorithm::split(lines, str, boost::is_any_of("\n"));

    for (size_t i = 0; i < lines.size(); i++) {
        boost::shared_ptr<Entry> entry(new Entry(lines[i]));
        entries.push_back(entry);
    }
}

SimpleScrollAdapter::Entry::Entry(const std::string& value) {
    this->value = value;
    this->charCount = value.size(); // u8len(value);
}

size_t SimpleScrollAdapter::Entry::GetLineCount(size_t width) {
    int full = (charCount / width);
    int remain = (charCount % width == 0 ? 0 : 1);
    return full + remain;
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

