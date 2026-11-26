#pragma once

#define WINTLS_USE_STANDALONE_ASIO
#include <wintls.hpp>
#include <websocketpp/config/core_client.hpp>
#include <websocketpp/transport/asio/endpoint.hpp>
#include <websocketpp/transport/asio/security/base.hpp>
#include <websocketpp/uri.hpp>

#include <memory>
#include <sstream>

namespace musik { namespace core { namespace net { namespace wintls_socket {
    namespace ws = websocketpp;
    namespace socket = websocketpp::transport::asio::socket;
    namespace lib = websocketpp::lib;

    /* WebSocket++'s built-in TLS policy is tied to OpenSSL. this connection
    adapts a Schannel stream to its socket policy so Windows clients can use
    the operating system's TLS implementation without linking OpenSSL. */
    class Connection : public lib::enable_shared_from_this<Connection> {
    public:
        using ptr = lib::shared_ptr<Connection>;
        using Socket = wintls::stream<asio::ip::tcp::socket>;
        using Strand = lib::shared_ptr<asio::io_context::strand>;
        using Context = lib::shared_ptr<wintls::context>;
        using ContextHandler = lib::function<Context(ws::connection_hdl)>;

        ptr get_shared() { return this->shared_from_this(); }
        bool is_secure() const { return true; }
        asio::ip::tcp::socket::lowest_layer_type& get_raw_socket() { return this->stream->next_layer().lowest_layer(); }
        asio::ip::tcp::socket& get_next_layer() { return this->stream->next_layer(); }
        Socket& get_socket() { return *this->stream; }
        void set_tls_init_handler(ContextHandler handler) { this->contextHandler = std::move(handler); }

        std::string get_remote_endpoint(lib::error_code& error) const {
            asio::error_code ec;
            const auto endpoint = this->stream->next_layer().remote_endpoint(ec);
            if (ec) {
                error = socket::make_error_code(socket::error::pass_through);
                return ec.message();
            }
            error = {};
            std::ostringstream text;
            text << endpoint;
            return text.str();
        }

        static lib::error_code translate_ec(lib::error_code error) { return error; }

    protected:
        lib::error_code init_asio(asio::io_context* io, Strand strand, bool) {
            if (!this->contextHandler) {
                return socket::make_error_code(socket::error::missing_tls_init_handler);
            }
            this->context = this->contextHandler(this->handle);
            if (!this->context) {
                return socket::make_error_code(socket::error::invalid_tls_context);
            }
            this->strand = std::move(strand);
            this->stream = std::make_shared<Socket>(*io, *this->context);
            return {};
        }
        void set_uri(ws::uri_ptr uri) { this->uri = std::move(uri); }
        void set_handle(ws::connection_hdl handle) { this->handle = std::move(handle); }
        void pre_init(socket::init_handler callback) {
            this->stream->set_server_hostname(this->uri->get_host());
            callback({});
        }
        void post_init(socket::init_handler callback) {
            this->error = socket::make_error_code(socket::error::tls_handshake_timeout);
            auto complete = [self = this->shared_from_this(), callback](const asio::error_code& ec) {
                self->error = ec ? socket::make_error_code(socket::error::tls_handshake_failed) : lib::error_code {};
                callback(self->error);
            };
            if (this->strand) {
                this->stream->async_handshake(wintls::handshake_type::client, this->strand->wrap(std::move(complete)));
            }
            else {
                this->stream->async_handshake(wintls::handshake_type::client, std::move(complete));
            }
        }
        lib::error_code get_ec() const { return this->error; }
        asio::error_code cancel_socket() {
            asio::error_code error;
            this->get_raw_socket().cancel(error);
            return error;
        }
        void async_shutdown(socket::shutdown_handler callback) {
            if (this->strand) {
                this->stream->async_shutdown(this->strand->wrap(std::move(callback)));
            }
            else {
                this->stream->async_shutdown(std::move(callback));
            }
        }

    private:
        Strand strand;
        Context context;
        std::shared_ptr<Socket> stream;
        ws::uri_ptr uri;
        ws::connection_hdl handle;
        ContextHandler contextHandler;
        lib::error_code error;
    };

    /* WebSocket++ creates a socket policy for each connection. pass it the
    client-selected TLS context before the asynchronous handshake begins. */
    class Endpoint {
    public:
        using socket_con_type = Connection;
        using socket_con_ptr = Connection::ptr;
        bool is_secure() const { return true; }
        void set_tls_init_handler(Connection::ContextHandler handler) { this->contextHandler = std::move(handler); }

    protected:
        lib::error_code init(socket_con_ptr connection) {
            connection->set_tls_init_handler(this->contextHandler);
            return {};
        }

    private:
        Connection::ContextHandler contextHandler;
    };

    /* use this client configuration when MUSIKCORE_USE_ASIO_WINTLS selects
    Schannel; other builds continue to use WebSocket++'s OpenSSL policy. */
    struct ClientConfig : ws::config::core_client {
        struct transport_config : ws::config::core_client::transport_config {
            using socket_type = Endpoint;
        };
        using transport_type = ws::transport::asio::endpoint<transport_config>;
    };
} } } }
