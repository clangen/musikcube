#include "library-query-backends.h"
#include "library-query-executors.h"
#include "library-storage.h"
#include "request.h"
#include "../common/error.h"
#include <musikcore/library/LocalLibraryBackend.h>
#include <musikcore/library/RemoteLibraryBackend.h>

using musik::core::LibraryBackend;

std::shared_ptr<const musik::core::library::Library> musik::core::ffi::CreateLibrary(const LibraryBackend::Shared& selected) {
    if (const auto local = std::dynamic_pointer_cast<library::LocalLibraryBackend>(selected)) {
        return detail::CreateLocalLibrary(local);
    }
    if (const auto remote = std::dynamic_pointer_cast<library::RemoteLibraryBackend>(selected)) {
        return detail::CreateRemoteLibrary(remote);
    }
    return {};
}

mc_result musik::core::ffi::OpenLibraryHandle(const LibraryBackend::Shared& library, mc_library** out) {
    return exception_barrier(nullptr, [&]() -> mc_result {
        if (!library || !out || *out) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        auto instance = CreateLibrary(library);
        if (!instance) {
            return MC_ERROR_IO;
        }
        *out = new mc_library { std::move(instance) };
        return MC_OK;
    });
}

musik::core::ffi::LibraryStatus musik::core::ffi::CaptureLibraryStatus(const LibraryBackend::Shared& selected) {
    LibraryStatus status;
    using ConnectionState = LibraryBackend::ConnectionState;
    status.kind = selected->GetType() == LibraryBackend::Type::Local ? MC_LIBRARY_LOCAL : MC_LIBRARY_REMOTE;
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
    if (const auto remote = std::dynamic_pointer_cast<library::RemoteLibraryBackend>(selected)) {
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
