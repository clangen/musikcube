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
#include <cursespp/curses_config.h>
#include <cursespp/Text.h>

#include <math.h>

#include <unordered_map>
#include <algorithm>

#define PAD(str, count) for (size_t i = 0; i < count; i++) { str += " "; }

namespace cursespp {
    namespace text {
        std::string Truncate(const std::string& str, size_t len) {
            /* not a simple substr anymore, gotta deal with multi-byte
            characters... */
            if (u8cols(str) > len) {
                auto prev = str.begin();
                auto it = str.begin();
                auto end = str.end();

                size_t cols = 0;
                while (cols <= len && it != str.end()) {
                    prev = it;

                    try {
                        utf8::next(it, end);
                    }
                    catch (...) {
                        /* invalid encoding, just treat as a single char */
                        ++it;
                    }

                    cols += u8cols(std::string(prev, it));
                }

                return std::string(str.begin(), prev);
            }

            return str;
        }

        std::string Ellipsize(const std::string& str, size_t len) {
            if (u8cols(str) > len) {
                std::string trunc = Truncate(str, len - 2);

                size_t tlen = u8cols(trunc);
                for (size_t i = tlen; i < len; i++) {
                    trunc += ".";
                }

                return trunc;
            }

            return str;
        }

        std::string Align(const std::string& str, TextAlign align, size_t cx) {
            size_t len = u8cols(str);

            if (len > cx) {
                return Ellipsize(str, cx);
            }
            else if (align == AlignLeft) {
                size_t pad = cx - len;
                std::string left = str;
                PAD(left, pad);
                return left;
            }
            else {
                size_t leftPad = (align == AlignRight)
                    ? (cx - len)
                    : (cx - len) / 2;

                size_t rightPad = cx - (leftPad + len);

                std::string padded;
                PAD(padded, leftPad);
                padded += str;
                PAD(padded, rightPad);
                return padded;
            }
        }

        /* not rocket science, but stolen from http://stackoverflow.com/a/1493195 */
        std::vector<std::string> Split(const std::string& str, const std::string& delimiters, bool trimEmpty) {
            using ContainerT = std::vector<std::string>;
            ContainerT tokens;
            std::string::size_type pos, lastPos = 0, length = str.length();

            using value_type = typename ContainerT::value_type;
            using size_type = typename ContainerT::size_type;

            while (lastPos < length + 1) {
                pos = str.find_first_of(delimiters, lastPos);
                if (pos == std::string::npos) {
                    pos = length;
                }

                if (pos != lastPos || !trimEmpty) {
                    tokens.push_back(value_type(
                        str.data() + lastPos,
                        (size_type) pos - lastPos));
                }

                lastPos = pos + 1;
            }

            return tokens;
        }

        inline void privateBreakLines(
            const std::string& line,
            size_t width,
            std::vector<std::string>& output)
        {
            size_t len = u8cols(line);
            size_t count = (int)ceil((float)len / (float)width);

            /* easy case: the line fits on a single line! */

            if (count <= 1) {
                output.push_back(line);
            }

            /* difficult case: the line needs to be split multiple sub-lines to fit
            the output display */

            else {
                /* split by whitespace */

                std::vector<std::string> words = Split(line, " \t\v\f\r");

                /* this isn't super efficient, but let's find all words that are greater
                than the width and break them into more sublines... it's possible to
                do this more efficiently in the loop below this with a bunch of additional
                logic, but let's keep things simple unless we run into performance
                problems! */

                std::vector<std::string> sanitizedWords;
                for (size_t i = 0; i < words.size(); i++) {
                    std::string word = words.at(i);
                    size_t len = u8cols(word);

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
                    size_t wordLength = u8cols(word);
                    size_t extra = (i != 0);

                    /* we have enough space for this new word. accumulate it. */

                    if (accumLength + extra + wordLength <= width) {
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

        std::vector<std::string> BreakLines(const std::string& line, size_t width) {
            std::vector<std::string> result;

            if (width > 0) {
                std::vector<std::string> split = Split(line, "\n");

                for (size_t i = 0; i < split.size(); i++) {
                    privateBreakLines(split.at(i), width, result);
                }
            }

            return result;
        }
    }

    namespace key {
       static std::unordered_map<std::string, std::string> KEY_MAPPING = {
            { "M-~",         "M-`" },
            { "M-bquote",    "M-`" },
            { "^@",          "M-`" },
            { "M-comma",     "M-," },
            { "M-stop",      "M-." },
            { "^H",          "KEY_BACKSPACE" },
            { "^?",          "KEY_BACKSPACE" },
            { "M-^H",        "M-KEY_BACKSPACE" },
            { "M-^?",        "M-KEY_BACKSPACE" },
            { "M-bksp",      "M-KEY_BACKSPACE" },
            { "^M",          "KEY_ENTER" },
            { "M-^M",        "M-enter" },
            { "kUP3",        "M-up" },
            { "kDN3",        "M-down" },
            { "M-KEY_UP",    "M-up" },
            { "M-KEY_DOWN",  "M-down" },
            { "kUP5",        "CTL_UP" },
            { "kDN5",        "CTL_DOWN" }
       };

        std::string Normalize(const std::string& kn) {
            auto it = KEY_MAPPING.find(kn);
            return (it != KEY_MAPPING.end()) ? it->second : kn;
        }

        std::string Read(int64_t ch) {
            std::string kn = keyname((int)ch);

            /* convert +ESC to M- sequences */
            if (kn == "^[") {
                int64_t next = getch();
                if (next != -1) {
                    kn = std::string("M-") + std::string(keyname((int)next));
                }
            }
        #ifdef WIN32
            /* transform alt->meta for uniform handling */
            else if (kn.find("ALT_") == 0) {
                std::transform(kn.begin(), kn.end(), kn.begin(), tolower);
                kn.replace(0, 4, "M-");
            }
        #endif
            /* multi-byte UTF8 character */
            else if (ch >= 194 && ch <= 223) {
                kn = "";
                kn += (char) ch;
                kn += (char) getch();
            }
            else if (ch >= 224 && ch <= 239) {
                kn = "";
                kn += (char) ch;
                kn += (char) getch();
                kn += (char) getch();
            }
            else if (ch >= 240 && ch <= 244) {
                kn = "";
                kn += (char) ch;
                kn += (char) getch();
                kn += (char) getch();
                kn += (char) getch();
            }

            kn = Normalize(kn);

#ifdef WIN32
            /* seems like on Windows using PDCurses, if a non-English keyboard
            is selected (like Russian) we receive a UTF16 character, not an
            encoded UTF8 character. in this case, let's convert it to a UTF8
            string and return that. */
            if (kn == "UNKNOWN KEY" && ch > 244) {
                kn = u16to8(std::wstring(1, (wchar_t)ch));
            }
#endif

            // std::cerr << "keyname: " << kn << std::endl;
            // std::cerr << "ch: " << ch << std::endl;

            return kn;
        }
    }
}
