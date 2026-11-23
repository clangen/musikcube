#include <stdio.h>
#include <stdlib.h>

/* record native unload during ordinary loader teardown. fatal tests can inspect
this evidence without calling loader APIs from an abort signal handler. */
static void note_unload(void) {
    if (getenv("MUSIKCORE_TEST_EXPECT_ABORT")) {
        fputs("fatal-test: image unloaded\n", stderr);
    }
}

#ifdef _WIN32
#include <windows.h>

/* the CRT invokes this fixture's entry point when its image is unmapped. */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void) instance;
    (void) reserved;
    if (reason == DLL_PROCESS_DETACH && reserved == NULL) {
        note_unload();
    }
    return TRUE;
}
#else
/* clang/gcc expose the native image destructor hook without C++ runtime state. */
__attribute__((destructor)) static void image_unloaded(void) {
    note_unload();
}
#endif
