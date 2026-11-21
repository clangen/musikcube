/* exercise the loader/sanitizer directly, without any core or SDK dependency. */
#include <dlfcn.h>
#include <cstring>

/* repeated loads must remain valid, while the optional final invalid access
must still be diagnosed by AddressSanitizer inside the instrumented module. */
int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        return 1;
    }
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        for (int library = 1; library < 3; ++library) {
            void* module = dlopen(argv[library], RTLD_NOW | RTLD_LOCAL);
            if (!module) {
                return 2;
            }
            auto name = reinterpret_cast<const char* (*)()>(dlsym(module, "module_name"));
            if (!name || std::strlen(name()) < 2) {
                return 3;
            }
            dlclose(module);
        }
    }
    if (argc == 4) {
        void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module) {
            return 4;
        }
        auto byte = reinterpret_cast<int (*)(unsigned)>(dlsym(module, "module_byte"));
        if (!byte) {
            return 5;
        }
        return byte(4);
    }
    return 0;
}
