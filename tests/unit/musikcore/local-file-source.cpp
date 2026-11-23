#include <common/FilesystemPath.h>
#include "../../support/plugins/PluginCandidates.h"
#include <musikcore/io/LocalFileSource.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace {
    /* exercise the same C calls a plugin makes, including in release builds. */
    void require(bool ok, const char* message) {
        if (!ok) {
            std::fprintf(stderr, "local file source: %s\n", message);
            std::abort();
        }
    }

    /* each open has its own cancellation latch and owns one file descriptor. */
    struct Source {
        mc_stream stream = MC_STREAM_DEFAULT;

        Source() { require(musik::core::io::CreateLocalFileSource(stream) == MC_OK, "create"); }
        ~Source() { stream.api->destroy(stream.instance); }
        Source(const Source&) = delete;
        Source& operator=(const Source&) = delete;
    };

    /* exercise create, short read, EOF, seek, and length through the same
    stream. a seek beyond 4 GiB checks offset width without allocating disk space. */
    void checkFile(mc_string_view path) {
        Source owner;
        auto& [size, file, api] = owner.stream;
        require(api->open(file, path, MC_STREAM_OPEN_CREATE | MC_STREAM_OPEN_READ |
            MC_STREAM_OPEN_WRITE | MC_STREAM_OPEN_TRUNCATE, nullptr) == MC_OK, "open");

        size_t count = 0;
        require(api->write(file, "music", 5, &count, nullptr) == MC_OK && count == 5, "write");
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        require(api->get_info(file, &info) == MC_OK && (info.flags & MC_STREAM_LENGTH_KNOWN) &&
            info.length_bytes == 5 && std::string_view(info.extension.data, info.extension.length) == "wav",
            "current length and normalized extension");

        require(api->seek(file, 0, nullptr) == MC_OK, "rewind");
        char data[10] {};
        mc_read_state state = MC_READ_MORE;
        require(api->read(file, data, sizeof(data), &count, &state, nullptr) == MC_OK &&
            count == 5 && std::memcmp(data, "music", 5) == 0, "short final read");
        require(api->read(file, data, sizeof(data), &count, &state, nullptr) == MC_OK &&
            count == 0 && state == MC_READ_END, "end of stream");

        uint64_t position = 0;
        constexpr uint64_t largeOffset = (UINT64_C(1) << 32) + 7;
        require(api->seek(file, largeOffset, nullptr) == MC_OK &&
            api->tell(file, &position) == MC_OK && position == largeOffset, "64-bit seek");
        require(api->seek(file, UINT64_MAX, nullptr) == MC_ERROR_INVALID_ARGUMENT &&
            api->tell(file, &position) == MC_OK && position == largeOffset, "reject unrepresentable offset");
        require(api->close(file) == MC_OK, "close");
    }

    /* Stop can arrive before open or between reads. neither case may revive
    the cancelled file or turn cancellation into an ordinary end of stream. */
    void checkInterrupt(mc_string_view path) {
        Source pending;
        pending.stream.api->interrupt(pending.stream.instance);
        require(pending.stream.api->open(pending.stream.instance, path, MC_STREAM_OPEN_READ, nullptr) ==
            MC_ERROR_CANCELLED, "cancel before open");

        Source opened;
        auto& [size, file, api] = opened.stream;
        require(api->open(file, path, MC_STREAM_OPEN_READ, nullptr) == MC_OK, "open existing");
        api->interrupt(file);
        api->interrupt(file);
        char data;
        size_t count = 42;
        mc_read_state state = MC_READ_MORE;
        require(api->read(file, &data, 1, &count, &state, nullptr) == MC_ERROR_CANCELLED && !count,
            "cancelled read");
        require(api->close(file) == MC_OK, "close cancelled file");
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "fixture directory");
    PluginCandidates files(musik::core::sdk::fs::pathFromUtf8(argv[1]));
    const auto filename = files.Directory("track.WAV");
    const mc_string_view path { filename.data(), filename.size() };
    checkFile(path);
    checkInterrupt(path);
}
