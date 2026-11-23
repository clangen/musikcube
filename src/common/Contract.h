#pragma once

#include <cstdio>
#include <exception>

namespace musik { namespace contract {

    /* enforce fatal invariants in Release too. this boundary cannot rely on
    process logging or user callbacks, which may be unavailable or reentrant. */
    inline void Assert(bool condition, const char* message) noexcept {
        if (!condition) {
            std::fputs("musikcore fatal contract violation: ", stderr);
            std::fputs(message, stderr);
            std::fputc('\n', stderr);
            std::terminate();
        }
    }

} }
