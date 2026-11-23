/* exercise the same helper with and without NDEBUG in every build configuration. */
#undef NDEBUG
#if MUSIKCORE_TEST_NDEBUG
#define NDEBUG 1
#endif

#include <common/Contract.h>
#include "../../support/FatalScope.h"
#include <cstring>

/* a failed invariant must report this caller, skip owner cleanup, and bypass
an embedding application's custom terminate handler installed by FatalScope. */
static void failContract(bool nullMessage) {
    FatalScope owner;
    std::fprintf(stderr, "expected-line=%u\n", static_cast<unsigned>(__LINE__ + 1));
    musik::contract::Assert(false, nullMessage ? nullptr : "test invariant");
}

/* successful checks evaluate once and never touch an optional diagnostic. */
int main(int argc, char** argv) {
    if (argc == 2) {
        failContract(std::strcmp(argv[1], "null") == 0);
        return 79;
    }
    unsigned evaluations = 0;
    musik::contract::Assert(++evaluations == 1, nullptr);
    return evaluations == 1 ? 0 : 1;
}
