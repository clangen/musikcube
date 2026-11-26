#pragma once

#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <vector>
#else
#include <dlfcn.h>
#endif

/* the caller supplies a symbol from the loaded plugin, so the path does not
depend on the application's working directory. */
inline std::filesystem::path ProjectMPresetDirectory(const void* symbol) {
#ifdef _WIN32
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(symbol), &module)) {
        return {};
    }
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) {
        return {};
    }
    return std::filesystem::path(path.data()).parent_path() / "projectM";
#else
    Dl_info info {};
    if (dladdr(symbol, &info) && info.dli_fname) {
        std::error_code error;
        const auto library = std::filesystem::absolute(info.dli_fname, error);
        if (!error) {
            return library.parent_path() / "projectM";
        }
    }
    return {};
#endif
}
