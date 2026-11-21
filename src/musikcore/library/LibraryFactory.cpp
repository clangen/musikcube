//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/db/Connection.h>
#include <musikcore/support/Common.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

using namespace musik::core;

static LibraryFactory::IMessageQueue* messageQueue = nullptr;
static std::shared_ptr<LibraryFactory> instance;

static library::RemoteLibrary::ConnectionOptions remoteOptionsFromPreferences() {
    using Remote = library::RemoteLibrary;
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
    options.configured = prefs->GetBool(keys::RemoteLibraryViewed, false);
    options.latencyTimeoutMs = prefs->GetInt(keys::RemoteLibraryLatencyTimeoutMs,
        Remote::Client::DefaultLatencyTimeoutMs);
    options.ignoreVersionMismatch = prefs->GetInt(keys::RemoteLibraryIgnoreVersionMismatch, false);
    if (prefs->GetBool(keys::RemoteLibraryTranscoderEnabled)) {
        options.transcoderBitrate = prefs->GetInt(keys::RemoteLibraryTranscoderBitrate);
        options.transcoderFormat = prefs->GetString(keys::RemoteLibraryTranscoderFormat);
    }
    return options;
}

void LibraryFactory::Initialize(IMessageQueue& messageQueue) {
    ::messageQueue = &messageQueue;
}

LibraryFactory& LibraryFactory::Instance() {
    if (!instance) {
        instance = std::shared_ptr<LibraryFactory>(new LibraryFactory());
    }
    return *instance;
};

LibraryFactory::LibraryFactory() {
    this->CreateLibrary("default-local-library", ILibrary::Type::Local);
    this->CreateLibrary("default-remote-library", ILibrary::Type::Remote);
}

LibraryFactory::~LibraryFactory() {
}

ILibraryPtr LibraryFactory::AddLibrary(int id, ILibrary::Type type, const std::string& name) {
    ILibraryPtr library;
    if (type == ILibrary::Type::Local) {
        library = library::LocalLibrary::Create(name, id, messageQueue);
    }
    else {
        auto remote = std::make_shared<library::RemoteLibrary>(name, id, messageQueue,
            remoteOptionsFromPreferences, true);
        remote->Connect();
        library = std::move(remote);
    }

    if (library) {
        this->libraries.push_back(library);
        this->libraryMap[id] = library;
        this->LibrariesUpdated();
    }

    return library;
}

void LibraryFactory::Shutdown() {
    if (instance) {
        for (ILibraryPtr library : instance->libraries) {
            library->Close();
        }
        instance->libraries.clear();
        instance->libraryMap.clear();
    }
}

ILibraryPtr LibraryFactory::CreateLibrary(const std::string& name, ILibrary::Type type) {
    auto prefs = Preferences::ForComponent(prefs::components::Libraries);
    std::vector<std::string> libraries;
    prefs->GetKeys(libraries);

    /* ensure the library doesn't already exist, and figure out a
    new unique identifier for this one... */

    int existingId = -1;
    int nextId = 0; /* we start at 1 because we always have. */
    for (size_t i = 0; i < libraries.size(); i++) {
        std::string n = libraries.at(i);
        int id = prefs->GetInt(name);

        if (n == name) {
            /* we already created a library with this name, let's go ahead
            and look it up and return it. */
            existingId = id;
            break;
        }

        if (id > nextId) {
            nextId = id;
        }
    }

    if (existingId != -1) {
        auto library = this->GetLibrary(existingId);
        if (!library) {
            return this->AddLibrary(existingId, type, name);
        }
    }

    ++nextId; /* unique */
    prefs->SetInt(name, nextId);

    return this->AddLibrary(nextId, type, name);
}

LibraryFactory::LibraryVector LibraryFactory::Libraries() {
    return LibraryFactory::Instance().libraries;
}

ILibraryPtr LibraryFactory::DefaultLocalLibrary() {
    return LibraryFactory::Instance().libraries.at(0);
}

ILibraryPtr LibraryFactory::DefaultRemoteLibrary() {
    return LibraryFactory::Instance().libraries.at(1);
}

ILibraryPtr LibraryFactory::DefaultLibrary(ILibrary::Type type) {
    return type == ILibrary::Type::Local
        ? DefaultLocalLibrary() : DefaultRemoteLibrary();
}

ILibraryPtr LibraryFactory::GetLibrary(int identifier) {
    if (identifier) {
        LibraryMap::iterator library = this->libraryMap.find(identifier);
        if (library != this->libraryMap.end()) {
            return library->second;
        }
    }
    return ILibraryPtr();
}

