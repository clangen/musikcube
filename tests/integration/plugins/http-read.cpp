#include <common/FilesystemPath.h>
#include "../../support/plugins/PluginCandidates.h"
#include <musikcore/ffi/plugin_abi/PluginRegistry.h>
#include <musikcore/plugin/settings.h>

#include <asio/ip/tcp.hpp>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>

using namespace std::chrono_literals;

namespace {
    constexpr size_t Chunk = 32768;
    constexpr std::string_view HttpId = "b153adad-ee98-4331-ad32-4ff7f34828cd";

    /* fail immediately so a missing publication cannot leave either test thread waiting. */
    void require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "progressive HTTP read: %s\n", message);
            std::abort();
        }
    }

    /* hold back each response segment until requested. the consumer must receive
    the first two before we close the HTTP connection and release the short tail. */
    void serve(asio::ip::tcp::acceptor& acceptor, std::future<void> append, std::future<void> finish) {
        asio::ip::tcp::iostream socket;
        acceptor.accept(socket.rdbuf()->socket());
        std::string line;
        while (std::getline(socket, line) && line != "\r") {
        }
        socket << "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n"
               << std::string(Chunk, 'A') << std::flush;
        require(append.wait_for(5s) == std::future_status::ready, "first chunk consumed before EOF");
        socket << std::string(Chunk, 'B') << std::flush;
        require(finish.wait_for(5s) == std::future_status::ready, "second chunk consumed before EOF");
        socket << 'C' << std::flush;
    }
}

/* each client loads the shipped provider and creates a stream before any disk
work. separate client processes can share the same application data directory. */
template<typename Work>
static void withHttpStream(const std::string& directory, const std::string& data, Work work,
    musik::core::ffi::PluginRegistry::Logger logger = {}) {
    using musik::core::ffi::PluginRegistry;
    PluginRegistry registry(std::move(logger));
    require(registry.Initialize(directory) == MC_OK, "load registry");
    const auto factory = registry.Find<mc_stream_factory>(HttpId);
    require(static_cast<bool>(factory), "find HTTP source");

    mc_plugin_preferences_service preferences = MC_PLUGIN_PREFERENCES_SERVICE_DEFAULT;
    preferences.read_integer = [](void*, const mc_integer_setting* field, int32_t* value) -> mc_result {
        const std::string_view key(field->key.data, field->key.length);
        *value = key == "precache_buffer_size_bytes" || key == "chunk_size_bytes" ? Chunk : field->default_value;
        return MC_OK;
    };
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.data_directory = { data.data(), data.size() };
    services.preferences = &preferences;
    mc_stream stream = MC_STREAM_DEFAULT;
    require(factory.Invoke([&](const auto& table) {
        stream.api = table.instance_api;
        return table.create(table.context, &services, &stream.instance, nullptr);
    }) == MC_OK, "create stream");
    work(stream);
    stream.api->destroy(stream.instance);
    require(registry.Shutdown() == MC_OK, "release registry");
}

int checkHttpCacheClient(const char* plugins, const char* directory, const std::string& url, char expected) {
    withHttpStream(plugins, directory, [&](const mc_stream& stream) {
        require(stream.api->open(stream.instance, { url.data(), url.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open shared-cache stream");
        std::array<char, Chunk> buffer;
        size_t total = 0;
        mc_read_state state = MC_READ_MORE;
        while (state != MC_READ_END) {
            size_t bytes = 0;
            require(stream.api->read(stream.instance, buffer.data(), buffer.size(), &bytes, &state, nullptr) == MC_OK,
                "read shared-cache stream");
            require(std::string_view(buffer.data(), bytes) == std::string(bytes, expected), "received another track");
            if (!total && bytes) {
                std::puts("ready");
                std::fflush(stdout);
            }
            total += bytes;
        }
        require(total == Chunk + 1, "complete cached track length");
    });
    return 0;
}

int checkProgressiveHttpRead(const char* pluginPath) {
    /* a sibling of the plugins directory preserves standalone ../lib lookup. */
    const auto dll = musik::core::sdk::fs::pathFromUtf8(pluginPath);
    PluginCandidates files(dll.parent_path().parent_path());
    files.Add(dll, "httpdatastream");
    const auto data = files.Directory("data");
    withHttpStream(files.Directory(), data, [&](const mc_stream& stream) {
        require(!fs::exists(musik::core::sdk::fs::pathFromUtf8(data)), "creating an unopened stream touched the filesystem");
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        Tcp::acceptor listener(sockets, { asio::ip::address_v4::loopback(), 0 });
        const auto url = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/progressive";
        std::promise<void> append, finish;
        std::thread writer(serve, std::ref(listener), append.get_future(), finish.get_future());

        require(stream.api->open(stream.instance, { url.data(), url.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open on the flushed prefix");

        std::array<char, Chunk + 1> buffer;
        size_t bytes = 0;
        mc_read_state state = MC_READ_MORE;
        const auto read = [&] {
            return stream.api->read(stream.instance, buffer.data(), buffer.size(), &bytes, &state, nullptr);
        };
        require(read() == MC_OK && bytes == Chunk && state == MC_READ_MORE &&
            std::string_view(buffer.data(), bytes) == std::string(Chunk, 'A'), "read a short published prefix");

        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        require(stream.api->get_info(stream.instance, &info) == MC_OK &&
            !(info.flags & MC_STREAM_LENGTH_KNOWN), "open-ended response has unknown length");
        require(stream.api->seek(stream.instance, Chunk + 1, nullptr) == MC_ERROR_UNSUPPORTED,
            "unknown-length stream cannot seek beyond published bytes");
        require(stream.api->seek(stream.instance, 0, nullptr) == MC_OK &&
            read() == MC_OK && bytes == Chunk &&
            std::string_view(buffer.data(), bytes) == std::string(Chunk, 'A'),
            "seek and reread published bytes while download is active");

        /* launch the read before releasing the server. a premature zero-byte
        result fails here; a reader waiting for EOF misses the next deadline. */
        std::promise<void> started;
        auto next = std::async(std::launch::async, [&] {
            started.set_value();
            return read();
        });
        started.get_future().wait();
        require(next.wait_for(50ms) == std::future_status::timeout, "read waits while the writer is behind");
        append.set_value();
        require(next.wait_for(5s) == std::future_status::ready && next.get() == MC_OK && bytes == Chunk &&
            state == MC_READ_MORE && std::string_view(buffer.data(), bytes) == std::string(Chunk, 'B'),
            "another published chunk wakes the read before EOF");

        finish.set_value();
        require(read() == MC_OK && bytes == 1 && buffer[0] == 'C' && state == MC_READ_END, "publish the short final tail");
        require(read() == MC_OK && !bytes && state == MC_READ_END, "EOF after the final byte");
        writer.join();
    });

    /* an interrupted stream must wake readers and seekers waiting for bytes
    the server has not yet sent. the server gate makes the stall reproducible. */
    withHttpStream(files.Directory(), data, [&](const mc_stream& stream) {
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        Tcp::acceptor listener(sockets, { asio::ip::address_v4::loopback(), 0 });
        const auto url = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/stalled";
        std::promise<void> release;
        auto released = release.get_future();
        std::thread server([&] {
            Tcp::iostream socket;
            listener.accept(socket.rdbuf()->socket());
            std::string line;
            while (std::getline(socket, line) && line != "\r") {}
            socket << "HTTP/1.0 200 OK\r\nContent-Length: 65536\r\nContent-Type: audio/test\r\n\r\n"
                   << std::string(Chunk, 'A') << std::flush;
            require(released.wait_for(5s) == std::future_status::ready, "release stalled server");
        });

        require(stream.api->open(stream.instance, { url.data(), url.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open published prefix before stall");
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        const auto infoStatus = stream.api->get_info(stream.instance, &info);
        require(infoStatus == MC_OK &&
            (info.flags & MC_STREAM_LENGTH_KNOWN) && info.length_bytes == 2 * Chunk &&
            std::string_view(info.content_type.data, info.content_type.length) == "audio/test",
            "publish response length and content type");
        std::array<char, Chunk> buffer;
        size_t bytes = 0;
        mc_read_state state = MC_READ_MORE;
        require(stream.api->read(stream.instance, buffer.data(), buffer.size(), &bytes, &state, nullptr) == MC_OK &&
            bytes == Chunk, "consume published prefix");
        std::promise<void> reading, seeking;
        auto reader = std::async(std::launch::async, [&] {
            size_t count = 0;
            mc_read_state readState = MC_READ_MORE;
            reading.set_value();
            return stream.api->read(stream.instance, buffer.data(), buffer.size(), &count, &readState, nullptr);
        });
        auto seeker = std::async(std::launch::async, [&] {
            seeking.set_value();
            return stream.api->seek(stream.instance, Chunk + 1, nullptr);
        });
        reading.get_future().wait();
        seeking.get_future().wait();
        require(reader.wait_for(50ms) == std::future_status::timeout &&
            seeker.wait_for(50ms) == std::future_status::timeout, "read and seek wait for missing bytes");
        stream.api->interrupt(stream.instance);
        require(reader.wait_for(2s) == std::future_status::ready && reader.get() == MC_ERROR_CANCELLED,
            "interrupt wakes blocked read");
        require(seeker.wait_for(2s) == std::future_status::ready && seeker.get() == MC_ERROR_CANCELLED,
            "interrupt wakes blocked seek");
        release.set_value();
        server.join();
    });

    /* redirects must publish metadata from the final response, not the
    intermediate response, and leave the complete body readable. */
    withHttpStream(files.Directory(), data, [&](const mc_stream& stream) {
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        Tcp::acceptor listener(sockets, { asio::ip::address_v4::loopback(), 0 });
        const auto base = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port());
        const auto url = base + "/redirect";
        std::thread server([&] {
            for (int index = 0; index < 2; ++index) {
                Tcp::iostream socket;
                listener.accept(socket.rdbuf()->socket());
                std::string line;
                while (std::getline(socket, line) && line != "\r") {}
                if (index == 0) {
                    socket << "HTTP/1.0 302 Found\r\nLocation: " << base
                           << "/final\r\nContent-Type: audio/wrong\r\nContent-Length: 0\r\n\r\n" << std::flush;
                }
                else {
                    socket << "HTTP/1.0 200 OK\r\nContent-Type: audio/final\r\nContent-Length: "
                           << Chunk << "\r\n\r\n" << std::string(Chunk, 'R') << std::flush;
                }
            }
        });
        require(stream.api->open(stream.instance, { url.data(), url.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open redirected response");
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        require(stream.api->get_info(stream.instance, &info) == MC_OK &&
            (info.flags & MC_STREAM_LENGTH_KNOWN) && info.length_bytes == Chunk &&
            std::string_view(info.content_type.data, info.content_type.length) == "audio/final",
            "publish final response metadata after redirect");
        std::array<char, Chunk> buffer;
        size_t bytes = 0;
        mc_read_state state = MC_READ_MORE;
        require(stream.api->read(stream.instance, buffer.data(), buffer.size(), &bytes, &state, nullptr) == MC_OK &&
            bytes == Chunk && std::string_view(buffer.data(), bytes) == std::string(Chunk, 'R'),
            "read redirected response body");
        server.join();
    });

    /* remote-track selectors carry credentials and media identity separately
    from the URL used for the network transfer. */
    withHttpStream(files.Directory(), data, [&](const mc_stream& stream) {
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        Tcp::acceptor listener(sockets, { asio::ip::address_v4::loopback(), 0 });
        const auto url = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/private";
        const auto selector = "musikcore://remote-track/{\"uri\":\"" + url +
            "\",\"originalUri\":\"original.flac\",\"type\":\".flac\",\"password\":\"password\"}";
        bool authorized = false;
        std::thread server([&] {
            Tcp::iostream socket;
            listener.accept(socket.rdbuf()->socket());
            std::string line;
            while (std::getline(socket, line) && line != "\r") {
                if (line == "Authorization: Basic ZGVmYXVsdDpwYXNzd29yZA==\r") {
                    authorized = true;
                }
            }
            socket << "HTTP/1.0 200 OK\r\nContent-Length: " << Chunk << "\r\n\r\n"
                   << std::string(Chunk, 'P') << std::flush;
        });
        require(stream.api->open(stream.instance, { selector.data(), selector.size() }, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open authenticated remote track");
        server.join();
        require(authorized, "remote track supplies basic credentials");
        mc_stream_info info = MC_STREAM_INFO_DEFAULT;
        require(stream.api->get_info(stream.instance, &info) == MC_OK &&
            std::string_view(info.uri.data, info.uri.length) == "original.flac" &&
            std::string_view(info.extension.data, info.extension.length) == "flac",
            "remote track preserves original identity and media type");
    });

    /* opening waits for the first published chunk. cancellation must return
    even when a server sends headers but never sends the body. */
    withHttpStream(files.Directory(), data, [&](const mc_stream& stream) {
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        Tcp::acceptor listener(sockets, { asio::ip::address_v4::loopback(), 0 });
        const auto url = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/open-stall";
        std::promise<void> accepted, release;
        auto released = release.get_future();
        std::thread server([&] {
            Tcp::iostream socket;
            listener.accept(socket.rdbuf()->socket());
            std::string line;
            while (std::getline(socket, line) && line != "\r") {}
            socket << "HTTP/1.0 200 OK\r\nContent-Length: 65536\r\n\r\n" << std::flush;
            accepted.set_value();
            require(released.wait_for(5s) == std::future_status::ready, "release stalled open server");
        });
        auto opening = std::async(std::launch::async, [&] {
            return stream.api->open(stream.instance, { url.data(), url.size() }, MC_STREAM_OPEN_READ, nullptr);
        });
        require(accepted.get_future().wait_for(2s) == std::future_status::ready, "server received open request");
        require(opening.wait_for(50ms) == std::future_status::timeout, "open waits for published bytes");
        stream.api->interrupt(stream.instance);
        require(opening.wait_for(2s) == std::future_status::ready && opening.get() == MC_ERROR_CANCELLED,
            "interrupt wakes blocked open");
        release.set_value();
        server.join();
    });

    std::string diagnostic;
    std::mutex diagnosticMutex;
    auto logger = [&](mc_log_level level, mc_string_view, mc_string_view message) {
        if (level == MC_LOG_ERROR) {
            std::lock_guard lock(diagnosticMutex);
            diagnostic.assign(message.data, message.length);
        }
    };
    withHttpStream(files.Directory(), data, [&](const mc_stream& stream) {
        asio::io_context sockets;
        using Tcp = asio::ip::tcp;
        Tcp::acceptor listener(sockets, { asio::ip::address_v4::loopback(), 0 });
        const auto url = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/secret-token";
        std::thread server([&] {
            Tcp::iostream socket;
            listener.accept(socket.rdbuf()->socket());
            std::string line;
            while (std::getline(socket, line) && line != "\r") {}
            socket << "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n" << std::flush;
        });
        const auto status = stream.api->open(stream.instance, { url.data(), url.size() }, MC_STREAM_OPEN_READ, nullptr);
        server.join();
        require(status == MC_ERROR_IO, "HTTP failure reaches caller");
        std::lock_guard lock(diagnosticMutex);
        require(diagnostic.find("404") != std::string::npos, "HTTP failure is logged with status");
        require(diagnostic.find("secret-token") == std::string::npos, "HTTP diagnostic excludes URL secrets");
    }, logger);
    return 0;
}
