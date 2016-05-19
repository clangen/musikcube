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