//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include "MasterLibrary.h"
#include "LibraryFactory.h"
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::runtime;

MasterLibrary::MasterLibrary() {
    this->LoadDefaultLibrary();
}

MasterLibrary::~MasterLibrary() {
}

int MasterLibrary::Enqueue(QueryPtr query, Callback cb) {
    return this->wrappedLibrary->Enqueue(query, cb);
}

int MasterLibrary::EnqueueAndWait(QueryPtr query, int64_t timeoutMs, Callback cb) {
    return this->wrappedLibrary->EnqueueAndWait(query, timeoutMs, cb);
}

IIndexer* MasterLibrary::Indexer() {
    return this->wrappedLibrary->Indexer();
}

int MasterLibrary::Id() {
    return this->wrappedLibrary->Id();
}

const std::string& MasterLibrary::Name() {
    return this->wrappedLibrary->Name();
}

void MasterLibrary::SetMessageQueue(IMessageQueue& queue) {
    this->wrappedLibrary->SetMessageQueue(queue);
}

IMessageQueue& MasterLibrary::GetMessageQueue() {
    return this->wrappedLibrary->GetMessageQueue();
}

MasterLibrary::IResourceLocator& MasterLibrary::GetResourceLocator() {
    return this->wrappedLibrary->GetResourceLocator();
}

bool MasterLibrary::IsConfigured() {
    return this->wrappedLibrary->IsConfigured();
}

MasterLibrary::ConnectionState MasterLibrary::GetConnectionState() const {
    return this->wrappedLibrary->GetConnectionState();
}

MasterLibrary::Type MasterLibrary::GetType() const {
    return this->wrappedLibrary->GetType();
}

void MasterLibrary::Close() {
    this->wrappedLibrary->Close();
}

void MasterLibrary::LoadDefaultLibrary() {
    std::unique_lock<decltype(this->libraryMutex)> lock(this->libraryMutex);

    auto prevWrappedLibrary = this->wrappedLibrary;

    auto prefs = Preferences::ForComponent(prefs::components::Settings);

    auto libraryType = (ILibrary::Type) prefs->GetInt(
        prefs::keys::LibraryType, (int) ILibrary::Type::Local);

    this->wrappedLibrary = LibraryFactory::Instance().DefaultLibrary(libraryType);

    if (prevWrappedLibrary != wrappedLibrary) {
        if (prevWrappedLibrary) {
            prevWrappedLibrary->QueryCompleted.disconnect(this);
            prevWrappedLibrary->ConnectionStateChanged.disconnect(this);
        }

        if (this->wrappedLibrary) {
            this->wrappedLibrary->QueryCompleted.connect(this, &MasterLibrary::OnQueryCompleted);
            this->wrappedLibrary->ConnectionStateChanged.connect(this, &MasterLibrary::OnConectionStateChanged);
        }

        this->LibraryChanged(prevWrappedLibrary, this->wrappedLibrary);
    }
}

void MasterLibrary::OnQueryCompleted(musik::core::db::IQuery* query) {
    this->QueryCompleted(query);
}

void MasterLibrary::OnConectionStateChanged(ConnectionState state) {
    this->ConnectionStateChanged(state);
}
