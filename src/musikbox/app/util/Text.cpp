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

#include <boost/lexical_cast.hpp>

namespace musik {
    namespace box {
        namespace text {
            void Truncate(std::string& str, size_t len) {
                /* not a simple substr anymore, gotta deal with multi-byte
                characters... */
                if (u8len(str) > len) {
                    std::string::iterator it = str.begin();
                    std::string::iterator end = str.end();

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

                    str = std::string(str.begin(), it);
                }
            }

            void Ellipsize(std::string& str, size_t len) {
                if (u8len(str) > len) {
                    Truncate(str, len - 2);
                    str += "..";
                }
            }

            std::string Duration(int seconds) {
                int mins = (seconds / 60);
                int secs = seconds - (mins * 60);
                return boost::str(boost::format("%d:%02d") % mins % secs);
            }

            std::string Duration(double seconds) {
                return Duration((int) round(seconds));
            }

            std::string Duration(std::string& str) {
                if (str.size()) {
                    int seconds = boost::lexical_cast<int>(str);
                    return Duration(seconds);
                }

                return "0:00";
            }
        }
    }
}
