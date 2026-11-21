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

#include "MasterLibrary.h"
#include "LibraryFactory.h"
#include <musikcore/ffi/consumer/library-binding.h>
#include <musikcore/ffi/consumer/library-storage.h>
#include <musikcore/ffi/consumer/playback-binding.h>
#include <musikcore/ffi/consumer/request.h>
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

int MasterLibrary::EnqueueAndWait(QueryPtr query, size_t timeoutMs, Callback cb) {
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

mc_result MasterLibrary::OpenLibrary(mc_library** out) const {
    std::lock_guard lock(this->libraryMutex);
    /* Pin the selected backend so requests and track references survive library switches. */
    if (const auto selected = std::dynamic_pointer_cast<LocalLibrary>(this->wrappedLibrary)) {
        return ffi::BindLocalLibrary(selected, out);
    }
    if (const auto remote = std::dynamic_pointer_cast<RemoteLibrary>(this->wrappedLibrary)) {
        return ffi::BindRemoteLibrary(remote, out);
    }
    return MC_ERROR_NOT_FOUND;
}

void MasterLibrary::LoadDefaultLibrary() {
    auto prefs = Preferences::ForComponent(prefs::components::Settings);
    const auto type = prefs->GetInt(prefs::keys::LibraryType, static_cast<int>(ILibrary::Type::Local));
    this->Select(type == static_cast<int>(ILibrary::Type::Remote) ? Type::Remote : Type::Local);
}

void MasterLibrary::Select(Type type) {
    std::lock_guard lock(this->libraryMutex);
    auto selected = LibraryFactory::Instance().DefaultLibrary(type);
    if (selected == this->wrappedLibrary) {
        return;
    }

    if (this->wrappedLibrary) {
        this->wrappedLibrary->QueryCompleted.disconnect(this);
        this->wrappedLibrary->ConnectionStateChanged.disconnect(this);
    }

    this->wrappedLibrary = std::move(selected);
    this->wrappedLibrary->QueryCompleted.connect(this, &MasterLibrary::OnQueryCompleted);
    this->wrappedLibrary->ConnectionStateChanged.connect(this, &MasterLibrary::OnConnectionStateChanged);
    this->PublishSelection(true);
}

void MasterLibrary::AttachContext(mc_context* context, audio::PlaybackService* playback) {
    std::lock_guard lock(this->libraryMutex);
    auto* indexer = LibraryFactory::Instance().DefaultLocalLibrary()->Indexer();
    if (this->context) {
        indexer->Started.disconnect(this);
        indexer->Progress.disconnect(this);
        indexer->Finished.disconnect(this);
    }
    this->context = context;
    this->playback = playback;
    if (context) {
        indexer->Started.connect(this, &MasterLibrary::OnIndexStarted);
        indexer->Progress.connect(this, &MasterLibrary::OnIndexProgress);
        indexer->Finished.connect(this, &MasterLibrary::OnIndexFinished);
    }
    this->PublishSelection(false);
}

void MasterLibrary::OnIndexStarted() {
    ffi::publish_indexer_event(*this->context, MC_INDEXER_STARTED, 0);
}

void MasterLibrary::OnIndexProgress(int updates) {
    ffi::publish_indexer_event(*this->context, MC_INDEXER_PROGRESS, updates);
}

void MasterLibrary::OnIndexFinished(int updates) {
    ffi::publish_indexer_event(*this->context, MC_INDEXER_FINISHED, updates);
}

void MasterLibrary::PublishSelection(bool selectionChanged) {
    if (!this->context) {
        return;
    }

    /* Invalidate playback before the dispatcher makes the new selection visible to views. */
    if (selectionChanged) {
        this->playback->Stop();
    }
    audio::DiscardUnavailablePlaybackQueue(*this->playback);

    auto status = ffi::CaptureLibraryStatus(this->wrappedLibrary);

    auto identity = status.state == MC_LIBRARY_CONNECTED
        ? ffi::CaptureLibrary(this->wrappedLibrary)
        : nullptr;
    ffi::publish_library_selection(*this->context, std::move(identity),
        std::move(status), selectionChanged);
}

void MasterLibrary::OnQueryCompleted(musik::core::db::IQuery* query) {
    this->QueryCompleted(query);
}

void MasterLibrary::OnConnectionStateChanged(ConnectionState state) {
    std::lock_guard lock(this->libraryMutex);
    this->PublishSelection(false);
    this->ConnectionStateChanged(state);
}
