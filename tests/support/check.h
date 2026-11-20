#pragma once

#include <stdio.h>

/* Report a failed expression without choosing the caller's cleanup strategy.
   Works in C11 and C++ tests without linking a framework or private core code. */
static inline int test_check(int passed, const char* expression, const char* file, int line) {
    if (!passed) {
        fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    }
    return passed;
}

/* Evaluate once. The caller can return, jump to cleanup, or accumulate failure. */
#define TEST_CHECK(expression) test_check((expression) != 0, #expression, __FILE__, __LINE__)
