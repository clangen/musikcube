/* zig's Linux shared-library links omit the CRT finalizer and DSO handle.
   Give each library its own handle so dlclose runs its C++ destructors. */
__attribute__((visibility("hidden"))) void *__dso_handle = &__dso_handle;

extern void __cxa_finalize(void *);

__attribute__((destructor)) static void finalize_shared_library(void) {
    __cxa_finalize(__dso_handle);
}
