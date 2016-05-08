#include "stdafx.h"

#include "SimpleScrollAdapter.h"
#include <boost/algorithm/string.hpp>
#include <utf8/utf8/unchecked.h>

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
        size_t count = (*it)->GetLineCount();

        for (size_t i = c; i < count; i++) {
            int64 attrs = (*it)->GetAttrs();

            if (attrs != 0) {
                wattron(window, attrs);
            }

            wprintw(window, "%s\n", (*it)->GetLine(i).c_str());

            if (attrs != 0) {
                wattroff(window, attrs);
            }

            --remaining;
        }

        ++it;
        c = 0;
    } while (it != end && remaining != 0);

}

void SimpleScrollAdapter::AddLine(const std::string& str, int64 attrs) {
    boost::shared_ptr<Entry> entry(new Entry(str));
    entry->SetWidth(this->width);
    entry->SetIndex(this->lineCount);
    entry->SetAttrs(attrs);
    entries.push_back(entry);
    this->lineCount += entry->GetLineCount();
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
}

SimpleScrollAdapter::Entry::Entry(const std::string& value) {
    this->value = value;
    this->charCount = value.size();
    this->width = -1;
}

size_t SimpleScrollAdapter::Entry::GetLineCount() {
    return max(1, this->lines.size());
}

std::string SimpleScrollAdapter::Entry::GetLine(size_t n) {
    return this->lines.at(n);
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

int64 SimpleScrollAdapter::Entry::GetAttrs() {
    return this->attrs;
}

void SimpleScrollAdapter::Entry::SetAttrs(int64 attrs) {
    this->attrs = attrs;
}

inline static int utf8Length(const std::string& str) {
    try {
        return utf8::distance(str.begin(), str.end());
    }
    catch (...) {
        return str.length();
    }
}

inline static void breakIntoSubLines(
    std::string& line,
    size_t width,
    std::vector<std::string>& output)
{
    size_t len = utf8Length(line);
    size_t count = (int) ceil((float) len / (float) width);

    /* easy case: the line fits on a single line! */

    if (count <= 1) {
        output.push_back(line);
    }

    /* difficult case: the line needs to be split multiple sub-lines to fit
    the output display */

    else {
        /* split by whitespace */

        std::vector<std::string> words;
        boost::algorithm::split(words, line, boost::is_any_of(" \t\v\f\r"));
        
        /* this isn't super efficient, but let's find all words that are greater
        than the width and break them into more sublines... it's possible to
        do this more efficiently in the following loop with a bunch of additional 
        logic, but let's keep things simple unless we run into performance
        problems! */

        std::vector<std::string> sanitizedWords;
        for (size_t i = 0; i < words.size(); i++) {
            std::string word = words.at(i);
            int len = std::distance(word.begin(), word.end());
            if (width >= len) {
                sanitizedWords.push_back(word);
            }
            else {
                std::string accum;
                
                /* ugh, we gotta split on UTF8 characters, not actual characters.
                this makes things a bit more difficult... */
                std::string::iterator begin = word.begin();
                std::string::iterator end = word.begin();
                int count = 0;
                bool done = false;
                while (end != word.end()) {
                    utf8::unchecked::next(end);
                    ++count;

                    if (count == width - 1 || end == word.end()) {
                        sanitizedWords.push_back(std::string(begin, end));
                        begin = end;
                        count = 0;
                    }
                }
            }
        }

        /* now we have a bunch of tokenized words. let's string them together 
        into sequences that fit in the output window's width */

        std::string accum;
        size_t accumLength = 0;

        for (size_t i = 0; i < sanitizedWords.size(); i++) {
            std::string word = sanitizedWords.at(i);
            size_t wordLength = utf8Length(word);
            size_t extra = (i != 0);

            /* we have enough space for this new word. accumulate it. the
            +1 here is to take the space into account */

            if (accumLength + extra + wordLength < width) {
                if (extra) {
                    accum += " ";
                }

                accum += word;
                accumLength += wordLength + extra;
            }

            /* otherwise, flush the current line, and start a new one... */

            else {
                output.push_back(accum);

                /* special case -- if the word is the exactly length of the
                width, just add it as a new line and reset... */

                if (wordLength == width) {
                    output.push_back(word);
                    accum = "";
                    accumLength = 0;
                }

                /* otherwise, let's start accumulating a new line! */

                else {
                    accum = word;
                    accumLength = wordLength;
                }
            }
        }

        if (accum.size()) {
            output.push_back(accum);
        }
    }
}

void SimpleScrollAdapter::Entry::SetWidth(size_t width) {
    if (this->width != width) {
        this->width = width;

        this->lines.clear();

        std::vector<std::string> split;
        boost::algorithm::split(split, this->value, boost::is_any_of("\n"));

        for (size_t i = 0; i < split.size(); i++) {
            breakIntoSubLines(split.at(i), this->width, this->lines);
        }
    }
}




