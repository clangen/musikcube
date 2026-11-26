#pragma once

#include <common/FilesystemPath.h>

#include <climits>
#include <fstream>
#include <string>
#include <vector>

/* libgme's filename APIs use narrow paths on Windows. read through the
filesystem path API so non-ASCII filenames work on every platform. */
inline bool ReadGmeFile(const std::string& path, std::vector<char>& bytes) {
    std::ifstream file(musik::core::sdk::fs::pathFromUtf8(path), std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const auto length = file.tellg();
    if (length <= 0 || length > LONG_MAX) {
        return false;
    }
    bytes.resize(static_cast<size_t>(length));
    file.seekg(0);
    return bool(file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())));
}
