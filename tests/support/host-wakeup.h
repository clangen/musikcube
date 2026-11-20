#ifndef MUSIKCORE_TEST_HOST_WAKEUP_H
#define MUSIKCORE_TEST_HOST_WAKEUP_H
#include <musikcore/context.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct test_host_event test_host_event;
/* Own a latched event so a wakeup before the host waits cannot be lost. */
test_host_event* test_host_event_create(void);
/* Destroy only after the associated context has stopped calling its hook. */
void test_host_event_destroy(test_host_event* event);
/* Match mc_wakeup_fn directly so C examples can sleep without a C++ adapter. */
void MC_CALL test_host_event_signal(void* event);
/* Consume one pending wakeup; return zero after a bounded wait instead of hanging. */
int test_host_event_wait(test_host_event* event);
#ifdef __cplusplus
}
#endif
#endif
