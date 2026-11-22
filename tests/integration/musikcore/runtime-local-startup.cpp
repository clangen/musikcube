/* start-local is an offline startup policy, not merely a late selection override.
the private URI provides a synchronous no-connect assertion; a loopback listener
then proves that explicit selection/reconnect and persisted remote startup still connect. */
#include <musikcore/musikcore.hpp>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include "../../support/check.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    using namespace musik::core;
    using namespace std::chrono_literals;

    /* keep accepted sockets open without answering the websocket handshake so
    an attempted connection cannot disappear before the owner observes it. */
    class ConnectionProbe {
        public:
            /* listen before context creation, so an eager connection cannot be missed. */
            ConnectionProbe()
            : acceptor(io, { asio::ip::address_v4::loopback(), 0 }) {
                this->Accept();
                this->worker = std::thread([this] {
                    this->io.run();
                });
            }

            /* retire callbacks before their captured owner and socket storage disappear. */
            ~ConnectionProbe() {
                this->io.stop();
                this->worker.join();
            }

            /* use an ephemeral port, independent of user servers or fixed fixtures. */
            unsigned short Port() const {
                return this->acceptor.local_endpoint().port();
            }

            /* accepted connections notify a bounded waiter; absence is asserted
            synchronously through the unconfigured client's URI instead of sleeping. */
            bool WaitFor(size_t expected) {
                std::unique_lock lock(this->mutex);
                return this->changed.wait_for(lock, 5s, [&] {
                    return this->accepted >= expected;
                });
            }

        private:
            /* rearm on the listener thread; destruction stops and joins it before
            the acceptor, sockets, or callback's captured owner can be destroyed. */
            void Accept() {
                auto socket = std::make_shared<asio::ip::tcp::socket>(this->io);
                this->acceptor.async_accept(*socket, [this, socket](const asio::error_code& error) {
                    if (!error) {
                        this->sockets.push_back(std::move(*socket));
                        {
                            std::lock_guard lock(this->mutex);
                            ++this->accepted;
                            this->changed.notify_all();
                        }
                        this->Accept();
                    }
                });
            }

            asio::io_context io;
            asio::ip::tcp::acceptor acceptor;
            std::vector<asio::ip::tcp::socket> sockets;
            std::mutex mutex;
            std::condition_variable changed;
            size_t accepted = 0;
            std::thread worker;
    };

    /* inspect only the connection-intent seam, since public selected-library
    status alone previously hid an unwanted connection to the background remote. */
    bool remoteUntouched() {
        const auto remote = std::static_pointer_cast<library::RemoteLibrary>(
            LibraryFactory::Instance().DefaultRemoteLibrary());
        return remote->WebSocketClient().Uri().empty();
    }
}

/* repeat startup in one isolated process to catch factory policy leaking
from the local override into a later normal remote session. */
int main() {
    ConnectionProbe probe;
    const auto settings = Preferences::ForComponent(prefs::components::Settings);
    settings->SetString(prefs::keys::RemoteLibraryHostname, "127.0.0.1");
    settings->SetInt(prefs::keys::RemoteLibraryWssPort, probe.Port());
    settings->SetBool(prefs::keys::RemoteLibraryWssTls, false);
    settings->SetBool(prefs::keys::RemoteLibraryViewed, true);
    settings->SetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Remote));

    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_LIBRARY;
    options.flags = MC_CONTEXT_START_LOCAL;
    {
        /* local creation and repeated local selection must leave the saved
        endpoint untouched, even when the persisted preferred catalog is remote. */
        api::Context context(options);
        if (!TEST_CHECK(context.GetLibraryStatus().kind == MC_LIBRARY_LOCAL) ||
            !TEST_CHECK(remoteUntouched()) ||
            !TEST_CHECK(context.SelectLibrary(MC_LIBRARY_LOCAL) == MC_OK) ||
            !TEST_CHECK(context.Dispatch() == MC_OK) || !TEST_CHECK(remoteUntouched())) {
            return 1;
        }

        /* opting into the remote catalog starts its deferred connection. */
        if (!TEST_CHECK(context.SelectLibrary(MC_LIBRARY_REMOTE) == MC_OK) ||
            !TEST_CHECK(probe.WaitFor(1)) || !TEST_CHECK(!remoteUntouched()) ||
            !TEST_CHECK(context.Close() == MC_OK)) {
            return 1;
        }
    }
    {
        /* reconnect is also an explicit network action while the selected
        catalog remains local; it must not require a remote-selection detour. */
        api::Context context(options);
        if (!TEST_CHECK(remoteUntouched()) ||
            !TEST_CHECK(context.ReconnectRemoteLibrary() == MC_OK) ||
            !TEST_CHECK(probe.WaitFor(2)) ||
            !TEST_CHECK(context.GetLibraryStatus().kind == MC_LIBRARY_LOCAL) ||
            !TEST_CHECK(context.Close() == MC_OK)) {
            return 1;
        }
    }
    {
        /* without the override, restoring a persisted remote selection retains
        its automatic connection behavior. */
        options.flags = 0;
        api::Context context(options);
        if (!TEST_CHECK(context.GetLibraryStatus().kind == MC_LIBRARY_REMOTE) ||
            !TEST_CHECK(probe.WaitFor(3)) || !TEST_CHECK(!remoteUntouched()) ||
            !TEST_CHECK(context.Close() == MC_OK)) {
            return 1;
        }
    }
    return 0;
}
