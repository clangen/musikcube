#pragma once

#include <string>
#include <string_view>

namespace musik { namespace core { namespace sdk {
    /* encode UTF-8 bytes as a single URL query value. */
    inline std::string EncodeUrlValue(std::string_view value) {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string result;
        for (const unsigned char byte : value) {
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
                byte == '-' || byte == '_' || byte == '.' || byte == '~')
            {
                result.push_back(static_cast<char>(byte));
            }
            else {
                result.push_back('%');
                result.push_back(hex[byte >> 4]);
                result.push_back(hex[byte & 15]);
            }
        }
        return result;
    }
} } }
