#pragma once

#include <filesystem>
#include <string_view>

namespace musik { namespace core { namespace sdk { namespace fs {

    static inline std::filesystem::path pathFromUtf8(std::string_view value) {
    #if defined(__cpp_char8_t)
        const auto* first = reinterpret_cast<const char8_t*>(value.data());
        return std::filesystem::path(first, first + value.size());
    #else
        return std::filesystem::u8path(value.begin(), value.end());
    #endif
    }

} } } }
