#ifndef MUSIKCORE_TEST_PROVIDER_PROBE_H
#define MUSIKCORE_TEST_PROVIDER_PROBE_H

#include <musikcore/plugin.h>

/* test-only typed capability: exercise a real call and host callbacks without
introducing a decoder or a permanent public feature merely to test loading. */
typedef struct test_probe_api {
    uint32_t struct_size;
    void* user_data;
    mc_result(MC_CALL* ping)(void*, int32_t*);
    mc_result(MC_CALL* register_again)(void*);
    mc_result(MC_CALL* make_error)(void*, mc_error**);
} test_probe_api;

#endif
