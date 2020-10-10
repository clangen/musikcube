//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/config.h>
#include <core/db/Connection.h>

#include <core/library/ILibrary.h>
#include <core/library/IIndexer.h>
#include <core/library/IQuery.h>
#include <core/library/QueryBase.h>

#include <mutex>

#include <sigslot/sigslot.h>

namespace musik { namespace core { namespace library {

    class MasterLibrary: public ILibrary, public sigslot::has_slots<> {
        public:
            MasterLibrary();
            virtual ~MasterLibrary();

            virtual int Enqueue(QueryPtr query, unsigned int options = 0, Callback = Callback()) override;
            virtual musik::core::IIndexer *Indexer() override;
            virtual int Id() override;
            virtual const std::string& Name() override;
            virtual void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            virtual musik::core::runtime::IMessageQueue& GetMessageQueue() override;
            virtual IResourceLocator& GetResourceLocator() override;
            virtual bool IsConfigured() override;
            virtual ConnectionState GetConnectionState() const override;
            virtual Type GetType() const override;
            virtual void Close() override;

            ILibraryPtr Wrapped() const { return Get(); }

            void LoadDefaultLibrary();

        private:

            ILibraryPtr Get() const;

            void OnQueryCompleted(musik::core::db::IQuery* query);
            void OnConectionStateChanged(ConnectionState state);

            ILibraryPtr wrappedLibrary;
            mutable std::recursive_mutex libraryMutex;
    };

} } }
