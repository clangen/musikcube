//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#pragma once

#include <musikcore/config.h>
#include <musikcore/db/Connection.h>

#include <musikcore/library/ILibrary.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/QueryBase.h>

#include <mutex>

#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace library {

    class MasterLibrary: public ILibrary, public sigslot::has_slots<> {
        public:
            sigslot::signal2<ILibraryPtr, ILibraryPtr> LibraryChanged;

            EXPORT MasterLibrary();
            EXPORT virtual ~MasterLibrary();

            EXPORT int Enqueue(QueryPtr query, Callback cb = Callback()) override;
            EXPORT int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback cb = Callback()) override;
            EXPORT musik::core::IIndexer *Indexer() override;
            EXPORT int Id() override;
            EXPORT const std::string& Name() override;
            EXPORT void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            EXPORT musik::core::runtime::IMessageQueue& GetMessageQueue() override;
            EXPORT IResourceLocator& GetResourceLocator() override;
            EXPORT bool IsConfigured() override;
            EXPORT ConnectionState GetConnectionState() const override;
            EXPORT Type GetType() const override;
            EXPORT void Close() override;

            EXPORT ILibraryPtr Wrapped() const noexcept { return this->wrappedLibrary; }

            EXPORT void LoadDefaultLibrary();

        private:

            void OnQueryCompleted(musik::core::db::IQuery* query);
            void OnConectionStateChanged(ConnectionState state);

            ILibraryPtr wrappedLibrary;
            mutable std::recursive_mutex libraryMutex;
    };

} } }
