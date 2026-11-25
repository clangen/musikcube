#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

/* preset filenames are UTF-8; fold ASCII letters without altering other bytes. */
inline bool ProjectMPresetMatches(std::string_view name, std::string_view query) {
    return std::search(name.begin(), name.end(), query.begin(), query.end(),
               [](char left, char right) {
                   return std::tolower(static_cast<unsigned char>(left)) ==
                       std::tolower(static_cast<unsigned char>(right));
               }) != name.end() ||
        query.empty();
}
