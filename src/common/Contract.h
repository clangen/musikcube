#pragma once

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace musik { namespace contract {

    namespace detail {
        /* fatal diagnostics are best-effort and bypass application loggers and
        terminate handlers. abort does not unwind unsafe owners or run atexit. */
        [[noreturn]] inline void Fail(const char* message, std::source_location location) noexcept {
            std::fprintf(stderr, "musikcore fatal contract violation at %.512s:%u (%.256s): %.2048s\n",
                location.file_name(), static_cast<unsigned>(location.line()), location.function_name(),
                message ? message : "unspecified invariant");
            std::abort();
        }
    }

    /* enforce fatal invariants in Release too. evaluate conditions exactly once;
    perform required state changes separately so the check stays observational. */
    inline void Assert(bool condition, const char* message,
        std::source_location location = std::source_location::current()) noexcept
    {
        if (!condition) {
            detail::Fail(message, location);
        }
    }

} }
