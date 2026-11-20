#include <cstdlib>
#include <new>

namespace { thread_local bool failNext = false; }
/* Arm a single allocation failure on the caller thread to exercise wrapper adoption cleanup. */
void TestFailNextAllocation() { failNext = true; }
/* Consume the one-shot failure flag or allocate ordinary storage without affecting worker threads. */
void* operator new(std::size_t size) {
    if (failNext) { failNext = false; throw std::bad_alloc(); }
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
/* Apply the same one-shot failure semantics to array allocation. */
void* operator new[](std::size_t size) { return ::operator new(size); }
/* Release storage from the malloc-backed unsized allocation override. */
void operator delete(void* value) noexcept { std::free(value); }
/* Release array storage from the malloc-backed allocation override. */
void operator delete[](void* value) noexcept { std::free(value); }
/* Handle compiler-selected sized deletion using the same allocator. */
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
/* Handle compiler-selected sized array deletion using the same allocator. */
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }
