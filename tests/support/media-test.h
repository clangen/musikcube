#ifndef MUSIKCORE_TEST_MEDIA_H
#define MUSIKCORE_TEST_MEDIA_H
#ifdef __cplusplus
extern "C" {
#endif
/* test-only control symbols stay outside the application and plugin SDKs. */
int media_fixture_load(const char* path);
int media_fixture_control(int operation);
int media_worker_start(int (*work)(void*), void* data);
int media_worker_join(void);
#ifdef __cplusplus
}
#endif
#endif
