#include "stdafx.h"
#include "Text.h"

#include <boost/lexical_cast.hpp>

namespace musik {
    namespace box {
        namespace text {
            void Truncate(std::string& str, size_t len) {
                if (str.size() > len) {
                    str = str.substr(0, len);
                }
            }

            void Ellipsize(std::string& str, size_t len) {
                if (str.size() > len) {
                    str = str.substr(0, len - 2) + "..";
                }
            }

            void Duration(std::string& str) {
                int n = boost::lexical_cast<int>(str);
                int mins = (n / 60);
                int secs = n - (mins * 60);
                str = boost::str(boost::format("%d:%02d") % mins % secs);
            }
        }
    }
}