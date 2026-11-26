#include "library-query-backends.h"
#include "library-query-executors.h"
#include "library-storage.h"
#include "request.h"
#include "../common/error.h"
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/library/RemoteLibrary.h>

using musik::core::ILibraryPtr;

std::shared_ptr<const LibraryInstance> musik::core::ffi::CreateLibraryInstance(const ILibraryPtr& selected) {
    if (const auto local = std::dynamic_pointer_cast<library::LocalLibrary>(selected)) {
        return detail::CreateLocalLibraryInstance(local);
    }
    if (const auto remote = std::dynamic_pointer_cast<library::RemoteLibrary>(selected)) {
        return detail::CreateRemoteLibraryInstance(remote);
    }
    return {};
}

mc_result musik::core::ffi::OpenLibraryHandle(const ILibraryPtr& library, mc_library** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        auto instance = CreateLibraryInstance(library);
        if (!instance) {
            return MC_ERROR_IO;
        }
        *out = new mc_library { std::move(instance) };
        return MC_OK;
    });
}

musik::core::ffi::LibraryStatus musik::core::ffi::CaptureLibraryStatus(const ILibraryPtr& selected) {
    LibraryStatus status;
    using ConnectionState = ILibrary::ConnectionState;
    status.kind = selected->GetType() == ILibrary::Type::Local ? MC_LIBRARY_LOCAL : MC_LIBRARY_REMOTE;
    status.configured = selected->IsConfigured();
    switch (selected->GetConnectionState()) {
        case ConnectionState::Disconnected:
            status.state = MC_LIBRARY_DISCONNECTED;
            break;
        case ConnectionState::Connecting:
            status.state = MC_LIBRARY_CONNECTING;
            break;
        case ConnectionState::Connected:
            status.state = MC_LIBRARY_CONNECTED;
            break;
        case ConnectionState::AuthenticationFailure:
            status.state = MC_LIBRARY_AUTHENTICATION_FAILED;
            break;
    }
    if (const auto remote = std::dynamic_pointer_cast<library::RemoteLibrary>(selected)) {
        const auto& socket = remote->WebSocketClient();
        status.endpoint = socket.Uri();
        status.serverVersion = socket.LastServerVersion();
        using Error = net::WebSocketClient::ConnectionError;
        switch (socket.LastConnectionError()) {
            case Error::None:
                break;
            case Error::ClosedByServer:
                status.error = MC_LIBRARY_CONNECTION_CLOSED;
                break;
            case Error::ConnectionFailed:
                status.error = MC_LIBRARY_CONNECTION_FAILED;
                break;
            case Error::InvalidPassword:
                status.error = MC_LIBRARY_CONNECTION_INVALID_PASSWORD;
                break;
            case Error::IncompatibleVersion:
                status.error = MC_LIBRARY_CONNECTION_INCOMPATIBLE_VERSION;
                break;
        }
    }

    return status;
}
