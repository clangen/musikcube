//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#if defined(__linux__) && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

#include "config.h"
#include "LruDiskCache.h"
#include <common/Filesystem.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <climits>

#ifdef _WIN32
#include <io.h>
#include <share.h>
#else
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {
    FILE* openFile(const fs::path& file, bool create) {
#ifdef _WIN32
        return _wfsopen(file.c_str(), create ? L"wbx" : L"rb", _SH_DENYNO);
#else
        return fopen(file.c_str(), create ? "wbx" : "rb");
#endif
    }

    constexpr std::string_view Pending = "musikcube-download-";
    std::atomic<uint64_t> nextTransfer { static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count()) };

    unsigned long processId() {
#ifdef _WIN32
        return GetCurrentProcessId();
#else
        return static_cast<unsigned long>(getpid());
#endif
    }

    /* a peer can still be writing while this process opens its first stream.
    remove a partial only when its owning process is known to have exited. */
    bool abandoned(std::string_view name) {
        name.remove_prefix(Pending.size());
        const auto dash = name.find('-');
        if (dash == std::string_view::npos || name.size() == dash + 1 ||
            name.find_first_not_of("0123456789", dash + 1) != std::string_view::npos)
        {
            return false;
        }
        unsigned long pid = 0;
        const auto end = name.data() + dash;
        const auto result = std::from_chars(name.data(), end, pid);
        if (result.ec != std::errc() || result.ptr != end || !pid || pid == processId()) {
            return false;
        }
#ifdef _WIN32
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) {
            return GetLastError() == ERROR_INVALID_PARAMETER;
        }
        const auto state = WaitForSingleObject(process, 0);
        CloseHandle(process);
        return state == WAIT_OBJECT_0;
#else
        return pid <= INT_MAX && kill(static_cast<pid_t>(pid), 0) < 0 && errno == ESRCH;
#endif
    }

    int64_t fileLength(FILE* reader) {
#ifdef _WIN32
        return _filelengthi64(_fileno(reader));
#else
        struct stat info {};
        return fstat(fileno(reader), &info) == 0 ? info.st_size : -1;
#endif
    }

    constexpr std::string_view Prefix = "musikcube_";

    /* the type is supplied by an HTTP peer and becomes part of a filename.
    keep MIME and extension spellings reversible for the existing disk format. */
    std::string filenameType(std::string type) {
        for (auto& c : type) {
            if (c == '/') {
                c = '-';
            }
            else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' ||
                         c == '.' || c == '-' || c == '_'))
            {
                return "unknown";
            }
        }
        return type.empty() || type.size() > 128 ? "unknown" : type;
    }
}

bool LruDiskCache::Initialize(const fs::path& directory, size_t maxEntries, const std::atomic<bool>& cancelled) {
    {
        std::lock_guard lock(mutex);
        if (cancelled) {
            return false;
        }
        if (root == directory) {
            if (limit != maxEntries) {
                limit = maxEntries;
                Prune();
            }
            return true;
        }
    }

    fs::create_directories(directory);
    std::vector<Entry> scanned;
    for (const auto& item : fs::directory_iterator(directory)) {
        if (cancelled) {
            return false;
        }
        std::error_code ec;
        const auto name = musik::core::sdk::fs::pathToUtf8(item.path().filename());
        if (!item.is_regular_file(ec)) {
            continue;
        }
        if (name.compare(0, Pending.size(), Pending) == 0) {
            if (abandoned(name)) {
                fs::remove(item.path(), ec);
            }
            continue;
        }
        /* an older running engine may own a .tmp file with no recorded PID. */
        if (name.compare(0, Prefix.size(), Prefix) != 0 || item.path().extension() == ".tmp") {
            continue;
        }
        const auto split = name.find('_', Prefix.size());
        if (split == std::string::npos || split + 1 == name.size()) {
            continue;
        }
        size_t id = 0;
        const auto result = std::from_chars(name.data() + Prefix.size(), name.data() + split, id);
        if (result.ec != std::errc() || result.ptr != name.data() + split) {
            continue;
        }
        auto type = name.substr(split + 1);
        /* completed MIME entries use a dash for the first slash. preserve
        subsequent dashes, e.g. audio/x-wav. */
        if (!type.empty() && type.front() != '.') {
            const auto slash = type.find('-');
            if (slash != std::string::npos) {
                type[slash] = '/';
            }
        }
        const auto used = item.last_write_time(ec);
        if (ec) {
            continue;
        }
        scanned.push_back({
            .id = id,
            .path = item.path(),
            .type = type,
            .used = used,
        });
    }
    /* a second worker can finish the scan and publish a download while this
    worker is still outside the mutex. retain those newer entries. */
    std::lock_guard lock(mutex);
    if (root != directory) {
        root = directory;
        entries.swap(scanned);
    }
    limit = maxEntries;
    Prune();
    return true;
}

FILE* LruDiskCache::OpenCached(size_t id, std::string& type, uint64_t& length) {
    std::lock_guard lock(mutex);
    auto it = std::find_if(entries.begin(), entries.end(), [id](const auto& entry) {
        return entry.id == id;
    });
    if (it == entries.end()) {
        return nullptr;
    }
    std::error_code ec;
    type = it->type;
    if (FILE* reader = openFile(it->path, false)) {
        /* a peer may replace the path while we open it. report the size of
        the file this reader actually owns. */
        const auto bytes = fileLength(reader);
        if (bytes >= 0) {
            length = static_cast<uint64_t>(bytes);
            it->used = fs::file_time_type::clock::now();
            fs::last_write_time(it->path, it->used, ec);
            return reader;
        }
        fclose(reader);
    }
    entries.erase(it);
    return nullptr;
}

bool LruDiskCache::CreatePartial(fs::path& path, FILE*& writer, FILE*& reader) {
    std::lock_guard lock(mutex);
    const auto prefix = std::string(Pending) + std::to_string(processId()) + "-";
    for (;;) {
        auto candidate = root / (prefix + std::to_string(++nextTransfer));
        writer = openFile(candidate, true);
        if (writer) {
            path.swap(candidate);
            reader = openFile(path, false);
            return reader != nullptr;
        }
        if (errno != EEXIST) {
            return false;
        }
    }
}

void LruDiskCache::Finish(size_t id, const fs::path& part, const std::string& type) {
    std::lock_guard lock(mutex);
    std::error_code ec;
    if (std::any_of(entries.begin(), entries.end(),
            [id](const auto& entry) {
                return entry.id == id;
            }))
    {
        return;
    }
    const auto final = root / ("musikcube_" + std::to_string(id) + "_" + filenameType(type));
    fs::rename(part, final, ec);
    if (ec) {
        return;
    }
    entries.push_back({
        .id = id,
        .path = final,
        .type = type,
        .used = fs::file_time_type::clock::now(),
    });
    Prune();
}

void LruDiskCache::Prune() {
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.used > b.used;
    });
    while (entries.size() > limit) {
        std::error_code ec;
        fs::remove(entries.back().path, ec);
        if (ec) {
            break;
        }
        entries.pop_back();
    }
}
