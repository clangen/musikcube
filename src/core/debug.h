#ifndef __C_AUTOM8_DEBUG_HPP__
#define __C_AUTOM8_DEBUG_HPP__

#include <sigslot/sigslot.h>
#include <string>

namespace musik {
    class debug {
    public:
        enum log_level {
            info = 0,
            warning,
            error
        };

        static void init();
        static void deinit();
        static sigslot::signal3<log_level, std::string, std::string> string_logged;
        static void log(log_level level, const std::string& tag, const std::string& string);
        static void log(const std::string& tag, const std::string& string);
    };
}

#endif