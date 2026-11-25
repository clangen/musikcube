#include <musikcore/abi.h>
#include <cstdlib>

/* discovery must never invoke the retired C++ entry point. */
extern "C" MC_PLUGIN_API void* GetPlugin() {
    std::abort();
}
