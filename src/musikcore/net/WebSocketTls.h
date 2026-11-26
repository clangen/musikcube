#pragma once

#include <memory>
#include <websocketpp/client.hpp>

/* select the TLS socket policy at build time so the WebSocket client and its
callers use the same interface on Windows and on OpenSSL-based builds. */
#if defined(MUSIKCORE_USE_ASIO_WINTLS)
#include <musikcore/net/WinTlsSocket.h>

namespace musik { namespace core { namespace net { namespace websocket_tls {
    using Client = websocketpp::client<wintls_socket::ClientConfig>;
    using Context = std::shared_ptr<wintls::context>;

    inline Context CreateContext() {
        return std::make_shared<wintls::context>(wintls::method::system_default);
    }
} } } }
#else
#include <websocketpp/config/asio_client.hpp>
#include <iostream>

namespace musik { namespace core { namespace net { namespace websocket_tls {
    using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
    using Context = std::shared_ptr<asio::ssl::context>;

    inline Context CreateContext() {
        auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);
        try {
            context->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
                asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use | asio::ssl::context::verify_none);
        }
        catch (std::exception& error) {
            std::cerr << "Error in context pointer: " << error.what() << std::endl;
        }
        return context;
    }
} } } }
#endif
