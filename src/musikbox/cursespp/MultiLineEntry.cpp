#include <stdafx.h>
#include <math.h>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "MultiLineEntry.h"

using namespace cursespp;

MultiLineEntry::MultiLineEntry(const std::string& value, int64 attrs) {
    this->value = value;
    this->charCount = value.size();
    this->width = -1;
    this->attrs = attrs;
}

size_t MultiLineEntry::GetLineCount() {
    return std::max((size_t) 1, this->lines.size());
}

std::string MultiLineEntry::GetLine(size_t n) {
    return this->lines.at(n);
}

std::string MultiLineEntry::GetValue() {
    return value;
}

size_t MultiLineEntry::GetIndex() {
    return this->index;
}

void MultiLineEntry::SetIndex(size_t index) {
    this->index = index;
}

int64 MultiLineEntry::GetAttrs() {
    return this->attrs;
}

void MultiLineEntry::SetAttrs(int64 attrs) {
    this->attrs = attrs;
}

inline static void breakIntoSubLines(
    std::string& line,
    size_t width,
    std::vector<std::string>& output)
{
    size_t len = u8len(line);
    size_t count = (int)ceil((float)len / (float)width);

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
        do this more efficiently in the loop below this with a bunch of additional
        logic, but let's keep things simple unless we run into performance
        problems! */

        std::vector<std::string> sanitizedWords;
        for (size_t i = 0; i < words.size(); i++) {
            std::string word = words.at(i);
            size_t len = u8len(word);

            /* this word is fine, it'll easily fit on its own line of necessary */

            if (width >= len) {
                sanitizedWords.push_back(word);
            }

            /* otherwise, the word needs to be broken into multiple lines. */

            else {
                std::string accum;

                /* ugh, we gotta split on UTF8 characters, not actual characters.
                this makes things a bit more difficult... we iterate over the string
                one displayable character at a time, and break it apart into separate
                lines as necessary. */

                std::string::iterator begin = word.begin();
                std::string::iterator end = word.begin();
                int count = 0;
                bool done = false;
                while (end != word.end()) {
                    utf8::unchecked::next(end);
                    ++count;

                    if (count == width || end == word.end()) {
                        sanitizedWords.push_back(std::string(begin, end));
                        begin = end;
                        count = 0;
                    }
                }
            }
        }

        words.clear();

        /* now we have a bunch of tokenized words. let's string them together
        into sequences that fit in the output window's width */

        std::string accum;
        size_t accumLength = 0;

        for (size_t i = 0; i < sanitizedWords.size(); i++) {
            std::string word = sanitizedWords.at(i);
            size_t wordLength = u8len(word);
            size_t extra = (i != 0);

            /* we have enough space for this new word. accumulate it. */

            if (accumLength + extra + wordLength < width) {
                if (extra) {
                    accum += " ";
                }

                accum += word;
                accumLength += wordLength + extra;
            }

            /* otherwise, flush the current line, and start a new one... */

            else {
                if (accum.size()) {
                    output.push_back(accum);
                }

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

void MultiLineEntry::SetWidth(size_t width) {
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
