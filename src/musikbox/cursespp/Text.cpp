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

#include "stdafx.h"
#include "Text.h"

#include <unordered_map>
#include <boost/lexical_cast.hpp>

#define PAD(str, count) for (size_t i = 0; i < count; i++) { str += " "; }

namespace cursespp {
    namespace text {
        std::string Truncate(const std::string& str, size_t len) {
            /* not a simple substr anymore, gotta deal with multi-byte
            characters... */
            if (u8cols(str) > len) {
                auto it = str.begin();
                auto end = str.end();

                size_t c = 0;
                while (c < len && it != str.end()) {
                    try {
                        utf8::next(it, end);
                    }
                    catch (...) {
                        /* invalid encoding, just treat as a single char */
                        ++it;
                    }

                    ++c;
                }

                return std::string(str.begin(), it);
            }

            return str;
        }

        std::string Ellipsize(const std::string& str, size_t len) {
            if (u8cols(str) > len) {
                return Truncate(str, len - 2) + "..";
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
    }

    namespace key {
       static std::unordered_map<std::string, std::string> KEY_MAPPING = {
            { "M-bquote",   "M-`" },
            { "^@",         "M-`" },
            { "M-comma",    "M-," },
            { "M-stop",     "M-." },
            { "^H",         "KEY_BACKSPACE" },
            { "^?",         "KEY_BACKSPACE" },
            { "M-^H",       "M-KEY_BACKSPACE" },
            { "M-^?",       "M-KEY_BACKSPACE" },
            { "M-bksp",     "M-KEY_BACKSPACE" },
            { "^M",         "KEY_ENTER" }
        };

        std::string Normalize(const std::string& kn) {
            auto it = KEY_MAPPING.find(kn);
            return (it != KEY_MAPPING.end()) ? it->second : kn;
        }
    }
}
