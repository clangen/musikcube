#include <musikcore/library.h>

#include "../common/error.h"
#include "../common/primitives.h"

#include "library-query-backends.h"
#include "request.h"

#include <musikcore/library/RemoteLibrary.h>
#include <common/runtime/MessageQueue.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace {
    using RemoteLibrary = musik::core::library::RemoteLibrary;
    using Queue = musik::core::runtime::MessageQueue;
    using State = RemoteLibrary::ConnectionState;

    mc_remote_state publicState(State state) {
        switch (state) {
            case State::Connected: return MC_REMOTE_CONNECTED;
            case State::Connecting: return MC_REMOTE_CONNECTING;
            case State::AuthenticationFailure: return MC_REMOTE_AUTHENTICATION_FAILED;
            case State::Disconnected: return MC_REMOTE_DISCONNECTED;
        }
        return MC_REMOTE_DISCONNECTED;
    }
}

struct mc_remote_session : sigslot::has_slots<> {
    explicit mc_remote_session(RemoteLibrary::ConnectionOptions options)
    : queue(std::make_shared<Queue>())
    , library(new RemoteLibrary("Remote", 0, this->queue.get(),
        [options = std::move(options)] { return options; }, false),
        [keepQueue = this->queue](RemoteLibrary* remote) { delete remote; })
    {
        this->library->ConnectionStateChanged.connect(this, &mc_remote_session::OnState);
        this->pump = std::thread([this] {
            while (this->queue->WaitForReady(this->stopping)) {
                this->queue->Dispatch();
            }
        });
    }

    ~mc_remote_session() {
        this->Disconnect();
        this->stopping.store(true);
        this->queue->WakeWaiters();
        this->pump.join();
        this->library->ConnectionStateChanged.disconnect(this);
        this->library->Close();
    }

    void Disconnect() {
        this->Complete(MC_ERROR_CANCELLED, MC_REMOTE_DISCONNECTED);
        this->library->Disconnect();
    }

    void OnState(State state) {
        const auto socket = this->library->WebSocketClient().ConnectionState();
        if (state == State::Connected && socket == RemoteLibrary::Client::State::Connected) {
            this->Complete(MC_OK, MC_REMOTE_CONNECTED);
        }
        else if (state != State::Connecting && socket == RemoteLibrary::Client::State::Disconnected) {
            this->Complete(MC_ERROR_IO, publicState(state));
        }
    }

    void Complete(mc_result status, mc_remote_state state) {
        std::function<void(mc_result, mc_remote_state)> finish;
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            finish = std::move(this->pending);
        }
        if (finish) {
            finish(status, state);
        }
    }

    const std::shared_ptr<Queue> queue;
    const std::shared_ptr<RemoteLibrary> library;
    std::mutex mutex;
    std::function<void(mc_result, mc_remote_state)> pending;
    std::atomic<bool> stopping { false };
    std::thread pump;
};

using namespace musik::core::ffi;

mc_result MC_CALL mc_remote_session_create(const mc_remote_options *options, mc_remote_session **out_session) {
    if (!out_session || *out_session || !options || options->struct_size < sizeof(*options) ||
        !valid_text(options->host, false) || !valid_text(options->password))
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&]() -> mc_result {
        RemoteLibrary::ConnectionOptions config;
        config.host.assign(options->host.data, options->host.length);
        if (options->password.length) {
            config.password.assign(options->password.data, options->password.length);
        }
        if (options->websocket_port) {
            config.websocketPort = options->websocket_port;
        }
        if (options->http_port) {
            config.httpPort = options->http_port;
        }
        config.websocketTls = options->websocket_tls;
        config.httpTls = options->http_tls;
        *out_session = new mc_remote_session(std::move(config));
        return MC_OK;
    });
}

void MC_CALL mc_remote_session_destroy(mc_remote_session **session) {
    if (session) {
        delete *session;
        *session = nullptr;
    }
}

mc_remote_state MC_CALL mc_remote_session_state(const mc_remote_session *session) {
    if (session) {
        return publicState(session->library->GetConnectionState());
    }
    return MC_REMOTE_DISCONNECTED;
}

mc_result MC_CALL mc_remote_session_connect_async(mc_context *context, mc_remote_session *session,
    mc_remote_connect_fn completion, void *user_data, mc_request_id *out_request)
{
    if (!out_request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *out_request = 0;
    if (!context || !session || !completion) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&]() -> mc_result {
        auto state = std::make_shared<std::atomic<mc_remote_state>>(MC_REMOTE_DISCONNECTED);
        auto finish = std::make_shared<std::function<void(mc_result)>>();
        std::function<void(mc_result, mc_remote_state)> publish =
            [state, finish](mc_result status, mc_remote_state connected) {
                state->store(connected);
                (*finish)(status);
            };
        {
            std::lock_guard<std::mutex> lock(session->mutex);
            if (session->pending) {
                return MC_ERROR_BUSY;
            }
            const auto status = reserve_external_request(context,
                [state, completion](mc_result result, void *data) {
                    const CompletionError error(result);
                    completion(result, state->load(), error.Get(), data);
                }, user_data, out_request, *finish);
            if (status != MC_OK) {
                return status;
            }
            session->pending = std::move(publish);
        }

        try {
            session->library->Connect();
            if (session->library->WebSocketClient().ConnectionGeneration()) {
                session->Complete(MC_OK, MC_REMOTE_CONNECTED);
            }
        }
        catch (...) {
            session->Complete(MC_ERROR_INTERNAL, MC_REMOTE_DISCONNECTED);
        }
        return MC_OK;
    });
}

mc_result MC_CALL mc_remote_session_disconnect(mc_remote_session *session) {
    if (!session) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&]() -> mc_result {
        session->Disconnect();
        return MC_OK;
    });
}

mc_result MC_CALL mc_remote_session_open_library(const mc_remote_session *session, mc_library **out_library) {
    if (!session || !out_library || *out_library) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return OpenLibraryHandle(session->library, out_library);
}
