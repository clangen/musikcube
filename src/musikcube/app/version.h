#pragma once

#include <string>

#define VERSION_MAJOR 0
#define VERSION_MINOR 51
#define VERSION_PATCH 0
#define VERSION "0.51.0"

namespace musik {
    namespace cube {
        static inline std::string userAgent() {
#ifdef WIN32
            static const std::string PLATFORM = "win32";
#elif defined __APPLE__
            static const std::string PLATFORM = "macos";
#else
            static const std::string PLATFORM = "linux";
#endif
            return
                "musikcube " +
                std::to_string(VERSION_MAJOR) + "." +
                std::to_string(VERSION_MINOR) + "." +
                std::to_string(VERSION_PATCH) +
                "(" + PLATFORM + ")";
        }
    }
}