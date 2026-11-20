#ifndef MUSIKCORE_TEST_PROFILE_H
#define MUSIKCORE_TEST_PROFILE_H
#include <stdint.h>
typedef struct test_profile_counts {
    uint64_t allocations;
    uint64_t requested_bytes;
    uint64_t copied_bytes;
} test_profile_counts;
#endif
