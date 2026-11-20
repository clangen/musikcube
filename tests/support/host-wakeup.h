#ifndef MUSIKCORE_TEST_HOST_WAKEUP_H
#define MUSIKCORE_TEST_HOST_WAKEUP_H
#include <musikcore/context.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct test_host_event test_host_event;
test_host_event* test_host_event_create(void);
void test_host_event_destroy(test_host_event* event);
void MC_CALL test_host_event_signal(void* event);
int test_host_event_wait(test_host_event* event);
#ifdef __cplusplus
}
#endif
#endif
