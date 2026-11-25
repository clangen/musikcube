#pragma once

#include <dlfcn.h>
#include <filesystem>

/* the caller supplies a symbol from the loaded plugin, so the path does not
depend on the application's working directory. */
inline std::filesystem::path ProjectMPresetDirectory(const void* symbol) {
    Dl_info info {};
    if (dladdr(symbol, &info) && info.dli_fname) {
        std::error_code error;
        const auto library = std::filesystem::absolute(info.dli_fname, error);
        if (!error) {
            return library.parent_path() / "projectM";
        }
    }
    return {};
}
