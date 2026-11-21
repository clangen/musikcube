/* deliberately invalid operations prove that C and C++ test entry points really
carry sanitizer instrumentation, rather than merely using a Debug build name. */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ordinary execution must succeed; each selected fault must produce a failing
process and the expected sanitizer diagnostic in the parent script. */
int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }

    if (strcmp(argv[1], "valid") == 0) {
        return 0;
    }

    if (strcmp(argv[1], "address") == 0) {
        volatile size_t offset = 8;
        char* bytes = (char*) malloc(8);
        if (!bytes) {
            return 3;
        }
        bytes[offset] = 1;
        free(bytes);
        return 0;
    }

    if (strcmp(argv[1], "undefined") == 0) {
        volatile int maximum = INT_MAX;
        volatile int overflow = maximum + 1;
        (void) overflow;
        return 0;
    }

    return 4;
}
