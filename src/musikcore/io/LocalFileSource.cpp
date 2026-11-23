/* stream offsets are 64-bit on 32-bit Linux too. select the matching libc
interfaces before any system header declares stat or seek. */
#if defined(__linux__) && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

#include "LocalFileSource.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <filesystem>
#include <new>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {
    /* use file descriptors so create, truncate, and append stay independent.
    seek and length use the large-file interfaces on Windows as well as Unix. */
#ifdef _WIN32
    using FileStat = struct _stat64;
    constexpr auto readFile = _read;
    constexpr auto writeFile = _write;
    constexpr auto seekFile = _lseeki64;
    constexpr auto closeFile = _close;
    constexpr auto statFile = _fstat64;

    int openFile(const std::filesystem::path& path, int flags) {
        return _wopen(path.c_str(), flags | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
    }

    bool regularFile(const FileStat& info) { return (info.st_mode & _S_IFMT) == _S_IFREG; }
#else
    using FileStat = struct stat;
    constexpr auto readFile = ::read;
    constexpr auto writeFile = ::write;
    constexpr auto seekFile = ::lseek;
    constexpr auto closeFile = ::close;
    constexpr auto statFile = ::fstat;
    static_assert(sizeof(off_t) >= sizeof(int64_t));

    int openFile(const std::filesystem::path& path, int flags) {
        /* an unexpected FIFO must not block open before we can check its type. */
        return ::open(path.c_str(), flags | O_NONBLOCK | O_CLOEXEC, 0666);
    }

    bool regularFile(const FileStat& info) { return S_ISREG(info.st_mode); }
#endif

    struct File {
        std::string uri;
        std::string extension;
        std::atomic<bool> interrupted { false };
        int fd = -1;
        mc_stream_open_flags flags = 0;
        bool attempted = false;

        ~File() {
            if (fd >= 0) {
                closeFile(fd);
            }
        }
    };

    mc_result status(const File& file) {
        if (file.interrupted.load()) {
            return MC_ERROR_CANCELLED;
        }
        return file.fd >= 0 ? MC_OK : MC_ERROR_INVALID_ARGUMENT;
    }

    mc_result MC_CALL open(void* instance, mc_string_view uri, mc_stream_open_flags flags, mc_error**) noexcept {
        auto& file = *static_cast<File*>(instance);
        if (file.interrupted.load()) {
            return MC_ERROR_CANCELLED;
        }
        constexpr auto writeOptions = MC_STREAM_OPEN_CREATE | MC_STREAM_OPEN_TRUNCATE | MC_STREAM_OPEN_APPEND;
        const bool read = flags & MC_STREAM_OPEN_READ;
        const bool write = flags & MC_STREAM_OPEN_WRITE;
        if (file.attempted || !uri.data || !uri.length || (!read && !write) || (!write && (flags & writeOptions)) ||
            (flags & ~(writeOptions | MC_STREAM_OPEN_READ | MC_STREAM_OPEN_WRITE)))
        {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        file.attempted = true;
        try {
            file.uri.assign(uri.data, uri.length);
            if (file.uri.find('\0') != std::string::npos) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            const auto path = std::filesystem::u8path(file.uri);
            const auto extension = path.extension().u8string();
            if (!extension.empty()) {
                file.extension.assign(extension.begin() + 1, extension.end());
                for (char& c : file.extension) {
                    if (c >= 'A' && c <= 'Z') {
                        c += 'a' - 'A';
                    }
                }
            }

            int mode = read ? (write ? O_RDWR : O_RDONLY) : O_WRONLY;
            if (flags & MC_STREAM_OPEN_CREATE) {
                mode |= O_CREAT;
            }
            if (flags & MC_STREAM_OPEN_TRUNCATE) {
                mode |= O_TRUNC;
            }
            if (flags & MC_STREAM_OPEN_APPEND) {
                mode |= O_APPEND;
            }
            file.fd = openFile(path, mode);
            if (file.fd < 0) {
                return errno == ENOENT ? MC_ERROR_NOT_FOUND : MC_ERROR_IO;
            }
            FileStat info {};
            if (statFile(file.fd, &info) != 0 || !regularFile(info)) {
                closeFile(file.fd);
                file.fd = -1;
                return MC_ERROR_IO;
            }
            file.flags = flags;
            return status(file);
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    mc_result MC_CALL close(void* instance) {
        auto& file = *static_cast<File*>(instance);
        const int fd = file.fd;
        file.fd = -1;
        return fd < 0 || closeFile(fd) == 0 ? MC_OK : MC_ERROR_IO;
    }

    void MC_CALL interrupt(void* instance) {
        /* only the worker owns the descriptor. closing it here could let an
        in-flight read operate on a descriptor reused by another thread. */
        static_cast<File*>(instance)->interrupted.store(true);
    }

    mc_result MC_CALL getInfo(void* instance, mc_stream_info* info) {
        auto& file = *static_cast<File*>(instance);
        const auto result = status(file);
        if (result != MC_OK) {
            return result;
        }
        FileStat stat {};
        if (statFile(file.fd, &stat) != 0) {
            return MC_ERROR_IO;
        }
        uint32_t flags = MC_STREAM_SEEKABLE | MC_STREAM_LENGTH_KNOWN | MC_STREAM_CAN_PREFETCH;
        if (file.flags & MC_STREAM_OPEN_READ) {
            flags |= MC_STREAM_READABLE;
        }
        if (file.flags & MC_STREAM_OPEN_WRITE) {
            flags |= MC_STREAM_WRITABLE;
        }
        *info = MC_STREAM_INFO_DEFAULT;
        info->flags = flags;
        info->length_bytes = static_cast<uint64_t>(stat.st_size);
        info->uri = { file.uri.data(), file.uri.size() };
        info->extension = { file.extension.data(), file.extension.size() };
        return MC_OK;
    }

    mc_result MC_CALL read(
        void* instance, void* dst, size_t capacity, size_t* bytes, mc_read_state* state, mc_error**)
    {
        auto& file = *static_cast<File*>(instance);
        *bytes = 0;
        *state = MC_READ_MORE;
        const auto result = status(file);
        if (result != MC_OK) {
            return result;
        }
        if (!(file.flags & MC_STREAM_OPEN_READ)) {
            return MC_ERROR_UNSUPPORTED;
        }
        if (capacity) {
            const auto count = static_cast<unsigned>(std::min<size_t>(capacity, INT_MAX));
            const auto n = readFile(file.fd, dst, count);
            if (n < 0) {
                return MC_ERROR_IO;
            }
            *bytes = static_cast<size_t>(n);
            *state = n ? MC_READ_MORE : MC_READ_END;
        }
        return MC_OK;
    }

    mc_result MC_CALL write(void* instance, const void* src, size_t requested, size_t* bytes, mc_error**) {
        auto& file = *static_cast<File*>(instance);
        *bytes = 0;
        const auto result = status(file);
        if (result != MC_OK) {
            return result;
        }
        if (!(file.flags & MC_STREAM_OPEN_WRITE)) {
            return MC_ERROR_UNSUPPORTED;
        }
        if (requested) {
            const auto count = static_cast<unsigned>(std::min<size_t>(requested, INT_MAX));
            const auto n = writeFile(file.fd, src, count);
            if (n <= 0) {
                return MC_ERROR_IO;
            }
            *bytes = static_cast<size_t>(n);
        }
        return MC_OK;
    }

    mc_result MC_CALL seek(void* instance, uint64_t position, mc_error**) {
        auto& file = *static_cast<File*>(instance);
        const auto result = status(file);
        if (result != MC_OK) {
            return result;
        }
        if (position > static_cast<uint64_t>(INT64_MAX)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return seekFile(file.fd, static_cast<int64_t>(position), SEEK_SET) < 0 ? MC_ERROR_IO : MC_OK;
    }

    mc_result MC_CALL tell(void* instance, uint64_t* position) {
        auto& file = *static_cast<File*>(instance);
        const auto result = status(file);
        if (result != MC_OK) {
            return result;
        }
        const auto offset = seekFile(file.fd, 0, SEEK_CUR);
        if (offset < 0) {
            return MC_ERROR_IO;
        }
        *position = static_cast<uint64_t>(offset);
        return MC_OK;
    }

    void MC_CALL destroy(void* instance) { delete static_cast<File*>(instance); }

    const mc_stream_vtable methods {
        .struct_size = sizeof(mc_stream_vtable),
        .destroy = destroy,
        .open = open,
        .close = close,
        .interrupt = interrupt,
        .get_info = getInfo,
        .read = read,
        .write = write,
        .seek = seek,
        .tell = tell,
    };
}

mc_result musik::core::io::CreateLocalFileSource(mc_stream& stream) {
    auto* file = new (std::nothrow) File;
    if (!file) {
        return MC_ERROR_INTERNAL;
    }
    stream = {
        .struct_size = sizeof(mc_stream),
        .instance = file,
        .api = &methods,
    };
    return MC_OK;
}
