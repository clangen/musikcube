#pragma once

#include "LocalLibraryBackend.h"
#include "RemoteLibraryBackend.h"

namespace musik { namespace core { namespace library {
    /* process-owned default catalogs. initialize before opening either backend;
    shutdown closes workers before releasing the process's ownership. retained
    library handles keep their closed backend and message queue alive. */
    void Initialize(runtime::IMessageQueue& queue);

    void Initialize(
        std::shared_ptr<runtime::IMessageQueue> queue, bool enableIndexer = true, bool connectRemote = true);

    void Shutdown();

    std::shared_ptr<LocalLibraryBackend> DefaultLocalBackend();

    std::shared_ptr<RemoteLibraryBackend> DefaultRemoteBackend();

    LibraryBackend::Shared DefaultBackend(LibraryBackend::Type type);
} } }