#include "pch.hpp"
#include "LibraryBackends.h"
#include <common/Contract.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <limits>
#include <stdexcept>

using namespace musik::core;

namespace {
    runtime::IMessageQueue* messageQueue = nullptr;
    std::shared_ptr<runtime::IMessageQueue> ownedQueue;
    std::shared_ptr<library::LocalLibraryBackend> localBackend;
    std::shared_ptr<library::RemoteLibraryBackend> remoteBackend;
    bool enableIndexer = true;
    bool connectRemote = true;

    struct LibraryLifetime {
        std::shared_ptr<runtime::IMessageQueue> queue;
        LibraryBackend::Shared library;
    };

    template<typename Backend> std::shared_ptr<Backend> retainQueue(std::shared_ptr<Backend> backend) {
        if (!ownedQueue) {
            return backend;
        }
        const auto lifetime = std::make_shared<LibraryLifetime>(LibraryLifetime { ownedQueue, backend });
        return std::shared_ptr<Backend>(lifetime, backend.get());
    }

    int identifierFor(const std::string& name, int reservedId) {
        const auto settings = Preferences::ForComponent(prefs::components::Libraries);
        std::vector<std::string> names;
        settings->GetKeys(names);
        int existing = 0, maximum = 0;
        for (const auto& key : names) {
            const int id = settings->GetInt(key);
            maximum = std::max(maximum, id);
            if (key == name) {
                existing = id;
            }
        }
        if (existing > 0 && existing != reservedId) {
            return existing;
        }
        if (maximum == std::numeric_limits<int>::max()) {
            throw std::overflow_error("library identifiers exhausted");
        }
        settings->SetInt(name, ++maximum);
        return maximum;
    }

    static library::RemoteLibraryBackend::ConnectionOptions remoteOptionsFromPreferences() {
        using Remote = library::RemoteLibraryBackend;
        using namespace musik::core::prefs;
        const auto prefs = Preferences::ForComponent(components::Settings);
        const auto port = [&](const std::string& key, unsigned short fallback) {
            const auto value = prefs->GetInt(key, fallback);
            return value > 0 && value <= UINT16_MAX ? static_cast<unsigned short>(value) : fallback;
        };

        Remote::ConnectionOptions options;
        options.host = prefs->GetString(keys::RemoteLibraryHostname, "127.0.0.1");
        options.password = prefs->GetString(keys::RemoteLibraryPassword, "");
        options.websocketPort = port(keys::RemoteLibraryWssPort, Remote::DefaultWebSocketPort);
        options.httpPort = port(keys::RemoteLibraryHttpPort, Remote::DefaultHttpPort);
        options.websocketTls = prefs->GetBool(keys::RemoteLibraryWssTls, false);
        options.httpTls = prefs->GetBool(keys::RemoteLibraryHttpTls, false);
        options.httpTlsAllowUntrustedCertificate =
            prefs->GetBool(keys::RemoteLibraryHttpTlsAllowUntrustedCertificate, false);
        options.configured = prefs->GetBool(keys::RemoteLibraryViewed, false);
        options.latencyTimeoutMs =
            prefs->GetInt(keys::RemoteLibraryLatencyTimeoutMs, Remote::Client::DefaultLatencyTimeoutMs);
        options.ignoreVersionMismatch = prefs->GetInt(keys::RemoteLibraryIgnoreVersionMismatch, false);
        if (prefs->GetBool(keys::RemoteLibraryTranscoderEnabled)) {
            options.transcoderBitrate = prefs->GetInt(keys::RemoteLibraryTranscoderBitrate);
            options.transcoderFormat = prefs->GetString(keys::RemoteLibraryTranscoderFormat);
        }
        return options;
    }

    void openDefaults() {
        if (localBackend && remoteBackend) {
            return;
        }
        musik::contract::Assert(messageQueue, "initialize library services before opening a catalog");
        auto local = std::static_pointer_cast<library::LocalLibraryBackend>(library::LocalLibraryBackend::Create(
            "default-local-library", identifierFor("default-local-library", 0), messageQueue, enableIndexer));
        auto remote = std::make_shared<library::RemoteLibraryBackend>("default-remote-library",
            identifierFor("default-remote-library", local->Id()), messageQueue, remoteOptionsFromPreferences, true);
        localBackend = retainQueue(std::move(local));
        remoteBackend = retainQueue(std::move(remote));
        if (connectRemote) {
            remoteBackend->Connect();
        }
    }
}

void musik::core::library::Initialize(runtime::IMessageQueue& queue) {
    ::enableIndexer = true;
    ::connectRemote = true;
    ::messageQueue = &queue;
}

void musik::core::library::Initialize(
    std::shared_ptr<runtime::IMessageQueue> queue, bool enableIndexer, bool connectRemote) {
    ::enableIndexer = enableIndexer;
    ::connectRemote = connectRemote;
    ownedQueue = std::move(queue);
    messageQueue = ownedQueue.get();
}

void musik::core::library::Shutdown() {
    if (localBackend) {
        localBackend->Close();
    }
    if (remoteBackend) {
        remoteBackend->Close();
    }
    localBackend.reset();
    remoteBackend.reset();
    ownedQueue.reset();
    messageQueue = nullptr;
    enableIndexer = true;
    connectRemote = true;
}

std::shared_ptr<library::LocalLibraryBackend> musik::core::library::DefaultLocalBackend() {
    openDefaults();
    return localBackend;
}

std::shared_ptr<library::RemoteLibraryBackend> musik::core::library::DefaultRemoteBackend() {
    openDefaults();
    return remoteBackend;
}

LibraryBackend::Shared musik::core::library::DefaultBackend(LibraryBackend::Type type) {
    return type == LibraryBackend::Type::Local ? LibraryBackend::Shared(DefaultLocalBackend()) : DefaultRemoteBackend();
}
