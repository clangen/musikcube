/* Test-only dyld interposition: observe dynamic allocator and byte-copy calls.
   No profiling symbols are linked into or exported by musikcore. */
#include "profile.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static atomic_bool enabled;
static atomic_uint_fast64_t allocations;
static atomic_uint_fast64_t requested_bytes;
static atomic_uint_fast64_t copied_bytes;
static atomic_bool calling_thread_only;
static _Atomic(pthread_t) calling_thread;

/* Reset counters while disabled, bind the optional caller-thread filter, then begin one measurement phase. */
static void begin_profile(int only_caller) {
    atomic_store(&enabled, 0);
    atomic_store(&allocations, 0);
    atomic_store(&requested_bytes, 0);
    atomic_store(&copied_bytes, 0);
    atomic_store(&calling_thread, pthread_self());
    atomic_store(&calling_thread_only, only_caller);
    atomic_store(&enabled, 1);
}
/* Enable one phase across all threads, including asynchronous query producers. */
__attribute__((visibility("default"))) void musikcore_test_profile_begin(void) { begin_profile(0); }
/* Enable one phase only on the calling thread to isolate wrapper submission overhead. */
__attribute__((visibility("default"))) void musikcore_test_profile_begin_thread(void) {
    begin_profile(1);
}
/* Disable measurement before copying atomic counters so reporting itself is not profiled. */
__attribute__((visibility("default"))) void musikcore_test_profile_end(test_profile_counts* counts) {
    atomic_store(&enabled, 0);
    counts->allocations = atomic_load(&allocations);
    counts->requested_bytes = atomic_load(&requested_bytes);
    counts->copied_bytes = atomic_load(&copied_bytes);
}
/* Count successful dynamic allocations only while the phase and optional thread filter allow them. */
static void allocated(void* result, size_t bytes) {
    if (result && atomic_load_explicit(&enabled, memory_order_acquire) &&
        (!atomic_load(&calling_thread_only) || pthread_equal(atomic_load(&calling_thread), pthread_self()))) {
        atomic_fetch_add_explicit(&allocations, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&requested_bytes, bytes, memory_order_relaxed);
    }
}
/* Forward malloc unchanged and observe the requested byte count after success. */
static void* profile_malloc(size_t size) {
    void* result = malloc(size); allocated(result, size); return result;
}
/* Forward calloc unchanged and observe its total requested storage after success. */
static void* profile_calloc(size_t count, size_t size) {
    void* result = calloc(count, size); allocated(result, count * size); return result;
}
/* Forward realloc unchanged and count successful resize requests as allocation activity. */
static void* profile_realloc(void* memory, size_t size) {
    void* result = realloc(memory, size); allocated(result, size); return result;
}
/* Accumulate out-of-line byte copies for the active phase and selected threads. */
static void copied(size_t size) {
    if (atomic_load_explicit(&enabled, memory_order_acquire) &&
        (!atomic_load(&calling_thread_only) || pthread_equal(atomic_load(&calling_thread), pthread_self()))) {
        atomic_fetch_add_explicit(&copied_bytes, size, memory_order_relaxed);
    }
}
/* Observe memcpy traffic while preserving the original operation's return value. */
static void* profile_memcpy(void* destination, const void* source, size_t size) {
    copied(size); return memcpy(destination, source, size);
}
/* Observe overlap-safe memmove traffic while preserving the original operation. */
static void* profile_memmove(void* destination, const void* source, size_t size) {
    copied(size); return memmove(destination, source, size);
}

__attribute__((used, section("__DATA,__interpose"))) static const struct {
    const void* replacement;
    const void* original;
} interposers[] = {
    {(const void*)&profile_malloc, (const void*)&malloc},
    {(const void*)&profile_calloc, (const void*)&calloc},
    {(const void*)&profile_realloc, (const void*)&realloc},
    {(const void*)&profile_memcpy, (const void*)&memcpy},
    {(const void*)&profile_memmove, (const void*)&memmove},
};
