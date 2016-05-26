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
                        }

                        ++it;
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
                int seconds = boost::lexical_cast<int>(str);
                return Duration(seconds);
            }
        }
    }
}
