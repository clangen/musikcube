#pragma once

#include <common/DeleteDefaults.h>
#include <common/FilesystemPath.h>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

/* own a unique build-tree directory so parallel/repeated runs cannot discover
one another's candidates. copies belong to this fixture; declare it before
the registry so DLLs unload before directory cleanup. cannot be copied or moved. */
class PluginCandidates {
public:
    MUSIK_DELETE_COPY_AND_MOVE(PluginCandidates);

    /* isolate files even when multiple CTest runs share the build tree. */
    explicit PluginCandidates(const fs::path& root)
    : directory(root / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) {
        fs::create_directories(root);
        if (!fs::create_directory(this->directory)) {
            throw std::runtime_error("fixture could not create a candidate directory");
        }
    }

    /* cleanup stays best-effort so it cannot obscure a test failure. */
    ~PluginCandidates() {
        std::error_code ignored;
        fs::remove_all(this->directory, ignored);
    }

    /* preserve the native suffix while controlling discovery order. these
    fixtures have no dynamic dependencies other than the platform runtime. */
    std::string Add(const fs::path& source, const std::string& name) {
        const auto destination = this->directory /
            musik::core::sdk::fs::pathFromUtf8(name + source.extension().string());
        fs::copy_file(source, destination);
        const auto bytes = destination.u8string();
        return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
    }

#ifndef _WIN32
    /* broken native candidates must not terminate discovery of later files.
    TODO: cover native Windows link/stat failures without requiring elevation. */
    void AddBrokenLink(const fs::path& source) {
        fs::create_symlink(
            this->directory / "missing-library", this->directory / ("00-broken" + source.extension().string()));
    }
#endif

    /* UTF-8 path bytes work with both C++17 and C++20 filesystem APIs. */
    std::string Directory(const char* child = nullptr) const {
        const auto path = child ? this->directory / child : this->directory;
        const auto bytes = path.u8string();
        return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
    }

private:
    fs::path directory;
};
