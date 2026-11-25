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

    mc_preferences_host_api preferences = MC_PREFERENCES_HOST_API_DEFAULT;
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
