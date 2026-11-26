#include <musikcore/net/RawWebSocketClient.h>

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        return 1;
    }
    asio::io_context io;
    musik::core::net::RawWebSocketClient client(io);
    client.SetMode(musik::core::net::RawWebSocketClient::Mode::TLS);
    bool opened = false;
    bool received = false;
    bool failed = false;
    const bool cancel = std::string(argv[2]) == "cancel";
    asio::steady_timer timer(io);
    client.SetOpenHandler([&](auto connection) {
        opened = true;
        if (cancel) {
            timer.expires_after(std::chrono::milliseconds(100));
            timer.async_wait([&](const auto&) {
                io.stop();
            });
            return;
        }
        client.Send(connection, "hello");
    });
    client.SetFailHandler([&](auto) {
        failed = true;
    });
    client.SetMessageHandler([&](auto, auto message) {
        received = message->get_payload() == "world";
        io.stop();
    });
    client.SetCloseHandler([&](auto) {
        io.stop();
    });
    client.Connect(std::string("wss://localhost:") + argv[1] + "/");
    client.Run();
    const bool expectFailure = std::string(argv[2]) == "fail";
    if (expectFailure ? !failed || opened : !opened || (!cancel && !received) || failed) {
        std::fprintf(stderr, "TLS WebSocket: unexpected lifecycle\n");
        return 1;
    }
    return 0;
}
