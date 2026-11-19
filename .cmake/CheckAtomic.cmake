include(CheckCXXSourceCompiles)

set(_ATOMIC_LINK_TEST_CODE "
#include <stdatomic.h>
#include <stdint.h>
typedef struct { uint64_t a; uint64_t b; } u128;
int main() {
  _Atomic(u128) v = { {0,0} };
  u128 tmp;

  // These map to __atomic_load/__atomic_store/__atomic_exchange for 16 bytes
  __atomic_load(&v, &tmp, __ATOMIC_SEQ_CST);
  __atomic_store(&v, &tmp, __ATOMIC_SEQ_CST);
  __atomic_exchange(&v, &tmp, &tmp, __ATOMIC_SEQ_CST);

  // Also hit a fetch-add on 8 bytes for good measure
  _Atomic(unsigned long long) x = 0;
  __atomic_fetch_add(&x, 1ULL, __ATOMIC_SEQ_CST);
  return (int)(x + tmp.a + tmp.b);
}
")

function(check_and_add_libatomic out_var)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    check_cxx_source_compiles("${_ATOMIC_LINK_TEST_CODE}" _ATOMIC_NO_LIB_OK)
    if(_ATOMIC_NO_LIB_OK)
      message(STATUS "[check-atomic] atomics link without libatomic")
    else()
      set(${out_var} ${${out_var}} atomic PARENT_SCOPE)
      message(STATUS "[check-atomic] added libatomic")
    endif()
  endif()
endfunction()
