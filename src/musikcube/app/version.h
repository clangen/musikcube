#pragma once

#include <string>

#define VERSION_MAJOR 0
#define VERSION_MINOR 65
#define VERSION_PATCH 1
#define VERSION "0.65.1"

namespace musik {
    namespace cube {
        static inline std::string userAgent() {
#ifdef WIN32
            static const std::string PLATFORM = "win32";
#elif defined __APPLE__
            static const std::string PLATFORM = "macos";
#elif defined __FreeBSD__
            static const std::string PLATFORM = "freebsd";
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
