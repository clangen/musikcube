#pragma once

#include <cstdint>
#include <string>

namespace musik { namespace core { namespace sdk {

    /* a scan records one entry per file so validation can reuse its timestamp
    and, when needed, inspect a changed container only once. */
    struct ScannedSubtrackFile {
        int64_t lastModified;
        int trackCount { -1 };
        int exists { -1 };
    };

    inline bool IsWithinDirectory(const std::string& filename, const std::string& directory) {
        if (directory.empty() || filename.size() <= directory.size() ||
            filename.compare(0, directory.size(), directory) != 0)
        {
            return false;
        }
        const char last = directory.back();
        return last == '/' || last == '\\' || filename[directory.size()] == '/' ||
            filename[directory.size()] == '\\';
    }

} } }
